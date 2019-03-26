# video-feedback
Simulated Video Feedback on the GPU

As a young child I ordered a quantity of mirrors from Edmund Scientific, including one
half-silvered mirror, and constructed a box from these mirrors in the hope of
peering though the half-silvered mirror and glimpsing the secrets of infinity.

Infinity viewed in this manner was disappointingly dim.

Have you grown tired of viewing YouTube videos of cats and musical groups?
Are twitch streams too much like quotidian reality for you?
Now you can watch endless hours of video of... video.
What could be more pataphysical?
And what could possibly be a more exalted use of YouTube's disk storage than
to store examples of simulated video feedback with random parameters?

I have always been fascinated by what happens when one points
a camera into a video monitor displaying the camera image.
For many configurations of camera rotation and zoom, the result can be
an endless complex and intriguing set of images, high-dimensional
chaos, also known as spatiotemporal chaos. [Crutchfield1984] [Crutchfield1988]

The resulting patterns can include those similar to Turing's reaction
diffusion PDEs [Turing1952], although in feedback the
spatial coupling is non-local.  And there is speculation that
the phenomenon of visual hallucination under the effects of psychedelic
drugs may involve a process ("frame stacking") analogous to video feedback in the neural
pathways of visual perception:
"When the precise frame refresh rate of perception is interrupted, consciousness
destabilizes and begins to track overlapping information from multiple
receding frames. Destabilization, feedback, and latency of frame refresh creates
sensory echo and complex frame interference patterns." [Kent2010].

Video feedback has to date been the subject of surprisingly
little scientific inquiry, and most of this has been qualitative.
With analog cameras and video, experiments are at best approximately replicable,
due to the effects of noise on chaos and the inability to precisely control parameters.
And digital simulations of the phenomenon, which are replicable, have been
tediously slow due to the large amount of computation required [Rampe2016]

With the arrival of GPUs and convenient libraries to program image processing
operations, the situation has changed and quantitative study of simulated
video feedback on the GPU, including an exploration of the space of
parameters (perhaps by the techniques of genetic algorithms) is now feasible.
Control of all parameters of the feedback process:
camera rotation angles, zoom, blur, noise level, image blending parameters, etc.
is precise and replicable in the simulation.  And the issue of the
feedback reaching a trivial fixed point of a completely white or black
screen can be avoided for most parameter settings by the application of a
histogram equalization.

The code uses the high performance and extremely well written ArrayFire library
[Yalamanchili2015] to implement in parallel the image processing operations needed for feedback
simulation.  Performance is adequate even on an entry level GPU,
approximately 30 frames/sec at full HD resolution of 1920x1080 pixels on an NVidia GTX860M.

Examples of runs with random parameters can be seen at http://rp.to/q3olh
although these videos suffer from compression artifacts not present when the code is run realtime.
The parameter settings are provided in the video comments, but some parameters
have changed since the early example runs.

Note that the code does not vary parameter settings during a simulation.
A vast number of addtional effects could be obtained with dynamic parameters.


DEPENDENCIES

ArrayFire GPU library Version 3.6.2.

An NVidia GPU with a sufficiently recent driver.
ArrayFire 3.6.2 is built using CUDA 10.0, and precompiled versions
of the ArrayFire library come with necessary CUDA 10.0 dependencies.
For CUDA 10 and 10.1 the minimum NVidia driver versions are:
    CUDA 10010, Linux(418.39), Windows(418.96)
    CUDA 10000, Linux(410.48), Windows(411.31)


COMPILING

This code has been tested only on Ubuntu 16.04.

After installing the ArrayFire libraries, the included Makefile
builds the fb executable.  Any recent versions of g++ are suitable.

Note that the LIBS variable in the Makefile can be set to compile
to run on the cpu without CUDA.  This is orders of magnitude slower
than CUDA, about 40x slower on my GTX860M laptop.


OPTIONS

The executable fb takes optional command line arguments which specify the
image processing operations to be performed for each frame of the feedback.
A display window is created with a random RGB image as the
initial image, and the simulated video feedback begins.

The lexical order of command line parameters specifies the
order in which the operations are performed.   For example:
    fb --blur=3 --roll=1 --zoom=1.01
will first apply a gaussian blur with a 3x3 mask, then apply a 1 degree roll
to the result of the blur, followed by a zoom by a factor of 1.01
before blending the result with the current frame according to the default
blend parameters.  Whereas:
    fb --zoom=1.01 --roll=1 --blur=3
will first zoom the image by a factor of 1.01 and then apply a roll of 1 degree,
followed by a blur with a 3x3 mask,
again before blending the result with the current frame according to the default
blend parameters.

Optional command line arguments are:

--rows=\<int\>

Specifies the number of rows in the display window.

--cols=\<int\>

Specifies the number of columns in the display window.

--blur=\<int\>

The size of the convolution mask for a gaussian blur.
Must be an odd integer, typically 3 or 5.  A value of 1 specifies no blur.
Blur simulates focus control of the camera in non-simulated video feedback.

--sharpen=\<float\>

A value of 1 specifies unsharp masking, a value >1 results in high-boost filtering,
emphasizing high spatial frequencies.
For both cases the gaussian kernel is hardcoded to size 3.

--roll=\<float\>

Rotate the virtual camera this many degrees around the viewing axis.

--blend=\<float\>

Controls the weightings applied to the current and previous image when blended.
A value of 0.0 retains the old image in its entirety, so there will be no
change to the initial random image regardless of the value of the other parameters.
A value of 1.0 discards the previous image, replacing it entirely with the new image.
The default value of 0.5 crossblends the old and new image with equal weights.

--zoom=\<float\>

Specifies how much to zoom in.  The default of 1.0 specifies no zoom.
A value of 1.01 will zoom in 1%.

--crawl=\<float\>,\<float\>,\<float\>,\<float\>

Pixel hue is interpreted as a polar angle, saturation and value combine
linearly and bilinearly with the other parameters to define a resampling
offset for each pixel, causing colors to crawl around.
The idea is adapted from http://erleuchtet.org/2011/06/white-one.html

--noise=\<float\>,\<float\>

The first value in the range [0,1] specifies how much noise is to be applied to
each pixel.  For each color channel of each pixel a random number [-1,1], scaled
by the first noise parameter, is added to that color value, if another independent
random number in the range [0,1] generated for that pixel exceeds the value of the 2nd supplied parameter.
So the first parameter can be thought of as the noise level, and the second a mutation
parameter indicating what fraction of the color values will be mutated by noise.

--histeq

Perform a histogram equalization.
Use of --histeq as the final parameter avoids the possibility of the
display converging to the fixed point of an all white or all black image.

--invert

Invert the value channel of the image in HSV space.

--depth=\<int\>

The default is color images (i.e. --depth=3), --depth=1 specifies greyscale.
    
--seed=\<int\>

A specific seed can be supplied to the random number generator with the optional --seed argument.
If this argument is not supplied, a random seed from the hardware entropy pool is used.
For runs to be repeatable, the same seed must be used.

--dump=\<string\>

This allows specification of a directory into which images should be saved.
When images are being saved, no display window is created.
Images are saved as .PNG files.

--nframes=\<int\>

This parameter specifies how many frames are to be saved to the directory specified
by the --dump parameter.


STOCHASTIC PARAMETERS

The Perl script fb_rand.pl selects random values for parameters in reasonable
ranges and then calls the fb executable.


BUGS

Command line argument parsing is ad hoc and brittle.

The roll operation generates excessive blur.  To see this, try:
    fb --roll=1
and note how the image blurs as rotation proceeds.
Something like a 3-pass shear operation in fourier space could be much better.


EXAMPLES

fb --crawl=1.47101180923741,-1.30636721107872,0.330758409810187,-0.896424485305786 --seed=180838769 --roll=159.553423379374  --invert --zoom=1.02490922469164 --blur=3 --blend=0.935182918689936 --sharpen=1.82351686590749 --histeq

fb --roll=313.103150683282 --zoom=1.4  --crawl=0.282938866108765,-0.682566848618407,-0.842619403029857,-1.99775540356571 --seed=3675620808 --sharpen=1.16410494334463 --blend=0.470024019142404 --noise=0.0715754031640216,0.00671502775216126 --blur=1 --histeq


REFERENCES

[Crutchfield1984] Crutchfield, J.P. "Space-Time Dynamics in Video Feedback". Physica 10D (1984) 229-245.

[Crutchfield1988] Crutchfield, J.P. "Spatio-Temporal Complexity in Nonlinear Image Processing",
IEEE Transactions on Circuits and Systmes, Vol. 35, No. 7, July, 1988

[Kent2010] Kent, J.L. "Psychedelic Information Theory: Shamanism in the Age of Reason", PIT Press 2010

[Rampe2016] Rampe, J.  https://softologyblog.wordpress.com/2016/10/24/video-feedback-simulation-version-3/

[Turing1952] Turing, A.M. "The Chemical Basis of Morphogenesis" Phil. Trans. of the Royal Society of London B.
237 (641): 37–72. doi:10.1098/rstb.1952.0012

[Yalamanchili2015] Yalamanchili, P., Arshad, U., Mohammed, Z., Garigipati, P., Entschev, P.,
Kloppenborg, B., Malcolm, James and Melonakos, J. (2015).
ArrayFire - A high performance software library for parallel computing with an
easy-to-use API. Atlanta: AccelerEyes. Retrieved from https://github.com/arrayfire/arrayfire
