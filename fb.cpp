
/*
 * fb.cpp
 * Simulated video feedback using the ArrayFire GPU library.
 * Examples:
 *	fb --blur=3 --roll=1 --zoom=1.01 --blend=0.1 --crawl=1.1,-0.5,1.4,-1.3 --noise=0.05,0.01 --seed=0 --histeq
 *	fb --roll=1 --zoom=1.01 --blend=0.1 --noise=0.05,1.0 --seed=1 --histeq --dump=images --nframes=10
 */

#include <stdio.h>
#include <arrayfire.h>
#include <time.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <unistd.h>

#define PI 3.14159
#define MAXPIPE 100		// maximum number of functions in pipeline (static allocation limit)

// default display window size
int rows=1080;
int cols=1920;

// for ArrayFire objects
using namespace af;

// the image we display
array image;

// feedback parameters
typedef struct param {
	int blur;				// size of gaussian convolution mask [1,3,5...]
	float sharpen;			// 1 for unsharp mask, >1 for highboost filter
	float roll;				// camera roll angle, degrees [0,360]
	float zoom;				// zoom in factor [1,1.5]
	float blend;			// blend coefficient [0,1]  or try out of range, which will clip
	float crawlds;			// simulated color crawl using 4 parameters
	float crawldv;
	float crawldsv;
	float crawld;
	float noise;			// noise level [0,1]
	float mutate;			// what proportion of pixels get noised [0,1]
	int depth;				// 1 for greyscale, 3 for rgb
	int seed;				// seed for ArrayFire RNG (if zero a random seed is used)
} Param;
Param param;				// the global parameters

// parameters used when saving frames to disk
char *dumpdir=NULL;			// directory into which to dump images, if non NULL
int nframes = 30;			// how many frames to dump

// Image processing pipeline stored as array of function pointers.
// These all take and return an image array, any parameters are globals in param struct
array (*pipeline[MAXPIPE])(array);
int npipe=0;				// how many functions in the pipeline

// ArrayFire display window
Window *win = NULL;

// for performance timing
clock_t t0,t1;

// 3d image array of constant 1.0, for clipping values
array one = constant(1.0, rows, cols, 3, f32);

// invert brightness
array invert(array img)
{
	// greyscale images are trivially inverted
	if ( param.depth == 1 )
		return 1.0 - img;

    // convert to HSV
    array hsv = colorSpace(img, AF_HSV, AF_RGB);

    // each channel, as a 2D array in [0,1]
    array h = hsv(span, span, 0);
    array s = hsv(span, span, 1);
    array v = hsv(span, span, 2);

    // invert brightness channel
    v = 1.0 - v;

    // convert back to RGB
    array out = colorSpace(join(2,h,s,v), AF_RGB, AF_HSV);

	return out;
}

// simulated color crawl
// (The idea is adapted from http://erleuchtet.org/2011/06/white-one.html)
array crawl(array img)
{
	if ( param.depth == 1 )
		return img;

	// convert to HSV
    array hsv = colorSpace(img, AF_HSV, AF_RGB);

	// each channel, as a 2D array in [0,1]
    array h = hsv(span, span, 0);
    array s = hsv(span, span, 1);
    array v = hsv(span, span, 2);

	// map H [0,1] into polar angle [0,2pi]
	array angle = 2.0 * PI * h;
	// compute the radius, using the parameters
	array dist =  param.crawlds*s + param.crawldv*v + param.crawldsv*s*v + param.crawld;

	// convert polar coords into x and y offsets for each pixel
	array coloff = dist * cos(angle);
	array rowoff = dist * sin(angle);

	// 2d matrix of col indices (unmodified)
	// 3x4 example:
	// 0 1 2 3
	// 0 1 2 3
	// 0 1 2 3
	array colidx = iota(dim4(1,cols), dim4(rows));

	// 2d matrix of row indices (unmodified)
	// 3x4 example:
    // 0 0 0 0
    // 1 1 1 0
    // 2 2 2 0
    array rowidx = iota(dim4(rows), dim4(1,cols));

	// apply the crawl to the indices
	colidx += coloff;
	rowidx += rowoff;

	// for output we lookup values of input array, indexed by the arrays of row and col indices
	return approx2(img, rowidx.as(f32), colidx.as(f32));
}

// Adds noise to image
// The mutate parameter indicates what proportion of pixels get noised,
// the noise parameter how noisy those pixels get.
array noise(array img)
{
	array mask;

	// noise affects positions where this mask is 1
	mask = randu(rows,cols,1) > param.mutate;

	if ( param.depth == 1 ) 
		return img + 2.0 * param.noise * mask * (randu(rows,cols,param.depth) - 0.5);
	else
		return img + 2.0 * param.noise * join(2,mask,mask,mask) * (randu(rows,cols,param.depth) - 0.5);
}

// clip values to 1.0
array clip(array img)
{
    // 2d mask showing where elements exceed 1.0
    array clip = img < 1.0;

	// replace those elements with 1.0
    replace(img, clip, one);

    return img;
}

// rotate image by given angle (degrees)
array roll(array img)
{
	//return rotate(img, param.roll * PI/180.0, true, AF_INTERP_BILINEAR);
	return rotate(img, param.roll * PI/180.0, true, AF_INTERP_BICUBIC);
}

// blend with previous image (global array, image)
array blend(array img)
{
    return param.blend*img + (1.0-param.blend)*image;
}

// blur by convolution with gaussian kernel of specified size
array blur(array img)
{   
    return convolve2(img, gaussianKernel(param.blur,param.blur));
}

// unsharp mask (from ArrayFire image_editing.cpp example)
// Blur kernel size hardcoded as 3 (could make this a different parameter)
array sharpen(array img)
{
    return img + param.sharpen * (img -  convolve2(img, gaussianKernel(3,3)));
}

// histogram equalization, image which can be RGB or greyscale
array histeq(array img)
{   
	// greyscale
	if ( param.depth == 1 ) {
		// scale into [0,255]
		array v = img * 255.f;
    
		// create histogram of intensity channel
		array histo = histogram(v,
			256,    // # bins
            0,      // minval
            255);   // maxval

    	// apply histogram equalization to intensity, and scale into [0,1]
    	return histEqual(v, histo) / 255.f;
	}

    // convert to HSV
    array img_hsv = colorSpace(img, AF_HSV, AF_RGB);

    // pointer to the intensity channel, scale into [0,254]
	// (Should scale to 255, but saveImage was normalizing some images to black, presumably because
	// roundoff caused some values to slightly exceed 255 ?)
    array v = img_hsv(span, span, 2) * 255.f;
    
    // create histogram of intensity channel
    array histo = histogram(v,
            256,    // # bins
            0,      // minval
            255);   // maxval
    
    // apply histogram equalization to intensity, and scale into [0,1]
    array vnorm = histEqual(v, histo) / 256.f;

    // replace the value channel in the HSV image with the normalized value
    img_hsv(span, span, 2) = vnorm;

    // convert back to RGB
    return colorSpace(img_hsv, AF_RGB, AF_HSV);
}

/*
 * zoom in a given factor by cropping and resizing
 */
array zoom(array in)
{
    int rows,cols;
    int shaverows,shavecols;

    // size of input image
    rows = (int)in.dims(0);
    cols = (int)in.dims(1);

    // how many pixels to shave off right and left
    shaverows = (int)(0.5 + 0.5*rows*(param.zoom-1.0));
    shavecols = (int)(0.5 + 0.5*cols*(param.zoom-1.0));

    array cropped = in(seq(shaverows, rows-shaverows-1), seq(shavecols, cols-shavecols-1), span);
    return resize(cropped, rows, cols);
}

// get random seed from entropy pool, used to initialize the ArrayFire RNG
int randseed()
{
	int randomfd;
	int seed;
	        
	if ( 0 > (randomfd = open("/dev/urandom",O_RDONLY)) ) {
		fprintf(stderr,"Couldn't open /dev/urandom\n");
		exit(-1);
	}
	if ( 4 != read(randomfd,&seed,4) ) {
		fprintf(stderr,"Couldn't read 4 bytes from /dev/urandom\n");
		exit(-1);
	}
	close(randomfd);

	return seed;
}

// execute the pipeline of image operations
array run(array in)
{
	int i;
	array out;

	for ( i=0 ; i<npipe ; i++ ) {
		out = (*pipeline[i])(in);
		in = out;
	}

	return in;
}

void scanargs(int argc, char *argv[])
{
    int i;
	char *c;

    for ( i=1 ; i<argc ; i++ ) {
        if ( c=strstr(argv[i],"rows") ) {
			rows = atof(c+5);
        }
        else if ( c=strstr(argv[i],"cols") ) {
			cols = atof(c+5);
        }
        else if ( c=strstr(argv[i],"blur") ) {
			param.blur = atoi(c+5);
			// a blur make of size 1 is a no op
			if ( param.blur != 1 )
				pipeline[npipe++] = blur;
        }
        else if ( c=strstr(argv[i],"sharpen") ) {
			param.sharpen = atof(c+8);
			pipeline[npipe++] = sharpen;
        }
        else if ( c=strstr(argv[i],"roll") ) {
			param.roll = atof(c+5);
			pipeline[npipe++] = roll;
        }
        else if ( c=strstr(argv[i],"blend") ) {
			param.blend = atof(c+6);
			pipeline[npipe++] = blend;
        }
        else if ( c=strstr(argv[i],"zoom") ) {
			param.zoom = atof(c+5);
			pipeline[npipe++] = zoom;
        }
        else if ( c=strstr(argv[i],"crawl") ) {
			if ( 4 != sscanf(c+6,"%f,%f,%f,%f",&param.crawlds,&param.crawldv,&param.crawldsv,&param.crawld) ) {
				fprintf(stderr,"Error, --crawl takes 4 comma-delimited floats\n");
				exit(-1);
			}
			pipeline[npipe++] = crawl;
        }
        else if ( c=strstr(argv[i],"noise") ) {
			if ( 2 != sscanf(c+6,"%f,%f",&param.noise,&param.mutate) ) {
				fprintf(stderr,"Error, --noise takes 2 comma-delimited floats\n");
				exit(-1);
			}
			pipeline[npipe++] = noise;
        }
        else if ( c=strstr(argv[i],"histeq") ) {
			pipeline[npipe++] = histeq;
        }
        else if ( c=strstr(argv[i],"invert") ) {
			pipeline[npipe++] = invert;
        }
        else if ( c=strstr(argv[i],"depth") ) {
			param.depth = atoi(c+6);
        }
        else if ( c=strstr(argv[i],"seed") ) {
			param.seed = atoi(c+5);
        }
        else if ( c=strstr(argv[i],"dump") ) {
			dumpdir = c+5;
        }
        else if ( c=strstr(argv[i],"nframes") ) {
			nframes = atoi(c+8);
        }
        else {
            fprintf(stderr,"Error, unknown parameter: %s\n",argv[i]);
            exit(-1);
        }
		if ( npipe >= MAXPIPE ) {
			fprintf(stderr,"Error, too many image operations, increase MAXPIPE\n");
            exit(-1);
		}
    }

	// sanity check parameters for sensible values
	if ( param.blur<1 || param.blur%2 != 1 ) {
		fprintf(stderr,"Error, --blur must be a positive odd integer\n");
		exit(-1);
	}
	if ( param.blend<0.0 || param.blend>1.0 ) {
		fprintf(stderr,"Error, --blend must be in the range [0.0,1.0]\n");
		exit(-1);
	}
	if ( param.zoom<1.0 || param.zoom>2.0 ) {
		fprintf(stderr,"Error, --zoom must be in the range [1.0,2.0]\n");
		exit(-1);
	}
	if ( param.noise<0.0 || param.noise>1.0 || param.mutate<0.0 || param.mutate>1.0 ) {
		fprintf(stderr,"Error, both --noise parameters must be in the range [0.0,1.0]\n");
		exit(-1);
	}
	if ( param.depth !=1 && param.depth != 3 ) {
		fprintf(stderr,"Error, --depth must be 3 (for RGB) or 1 (for greyscale)\n");
		exit(-1);
	}
}


int main(int argc, char** argv)
{
	int frame=0;

	// default parameter values
    param.blur = 1;
    param.sharpen = 0.0;
    param.roll = 0.0;
    param.zoom = 1.0;
    param.blend = 0.5;
    param.crawlds = 0.0;
    param.crawldv = 0.0;
    param.crawldsv = 0.0;
    param.crawld = 0.0;
    param.noise = 0.0;
    param.mutate = 0.0;
    param.depth = 3;
    param.seed = 0;

	// process cmdline args
	scanargs(argc,argv);

	// if --seed was specified (i.e. is not the default value of zero),
	// initialize ArrayFire random number generator with a random seed from the entropy pool
	if ( param.seed==0 ) {
		param.seed = randseed();
	}
	setSeed(param.seed);
	setDefaultRandomEngineType(AF_RANDOM_ENGINE_THREEFRY);

	// if not dumping frames to files, open the display window
	if ( !dumpdir ) {
		// create ArrayFire display window
    	win = new Window(cols,rows,"feedback");
	}

	// Initial RGB image, random colors.
	// Alternatively, could load an initial image from a file like this:
	//image = loadImage("/tmp/plus.jpg", true) / 255.f;
	image = randu(rows,cols,param.depth);
	
	t0 = clock();

	// loop until window closed or enough frames dumped
    while ( (dumpdir && frame<nframes) || (!dumpdir && !win->close()) ) {

		// execute the pipeline of image processing operations
		image = run(image);

        // dump or display image
        if ( dumpdir ) {
			char buf[80];
            sprintf(buf,"%s/frame%05d.png",dumpdir,frame);
            saveImage(buf, clip(image));
		}
		else {
			// render array to the window (single buffered)
        	win->image(image);
        	win->show();
		}
			
		frame++;
	}

	t1 = clock();
	fprintf(stderr, "fps %g\n",frame/(((float)(t1-t0))/(float)CLOCKS_PER_SEC));
}
