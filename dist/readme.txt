This is a script that handles all the steps of conversion from a video file to a TI playable video.
(11/4/2018)

Copy the video file into the same folder as "tividconvert.exe", then run:

tividconvert <video file>

Make sure there are NO SPACES in your path. It doesn't like spaces. No quotes, either. Sorry!

Fair warning! The process will fail after all the long term video processing if there is no audio track. So make sure your video file also has audio! Just add a blank audio track if you have to. :)

You must have permission to write a temporary folder in the current folder, and the system will need about 1MB per second of video for temporary files. It will take a fair bit of time, but runs the following steps:

ffmpeg - to extract frames from the video file
mogrify - part of Imagemagick - handles the image resizing
convert9918 - in conjuction with dircmd (one thread per CPU core) to convert the images to TI Artist format
- a cleanup of temporary files, to reduce disk usage
ffmpeg - again, to extract the audio track
sox - to convert the audio to 8-bit unsigned
audioconvert - to convert the 8-bit audio to TI sound chip commands
videopack - to pack the TI Artist images and TI sound chip audio into a packed TI video file
cartrepack - to convert the packed file into a TI cartridge file
- the temporary folder is then deleted

If any errors occur, the temporary folder is not deleted, so you can inspect it. Delete it before you run it again.

There are a couple of notes. First, the output file (finalPACK.bin) can only be played by Classic99 at this time, as a "big file hack" file. You need to run the "videoblaster" object file, with the program name MAIN, to play it. This can in theory be any size, but note it's pre-loaded entirely into RAM, so your system RAM is an issue.

The cartridge file (finalPACK8.bin) is a 378 bank-switch type cartridge. It's padded to a power of two multiple, but, note that this bank switch mechanism has a total possible size of 32MB. The maximum hardware cartridge we've built today is only 2MB. But this tool will build a cart up to 512MB,
using the Gigacart banking (which just adds 4 data bits as the next most significant latch bits).

Note: The Convert9918 stage might display corrupt looking text depending on how many cores are executing at the same time - there's no flow control between the processes to the console.

TIP: If you want to use non-default settings for Convert9918, open the tool and set the parameters you'd like to use (for instance, I like to use "Order2" dithering and adjust the gamma). Make sure you load (any) image to test, then quit the tool. Copy the created 'convert9918.ini' to the same folder as tividconvert.exe, and (IMPORTANT!) set it to 'read-only'. The image conversions will then use the settings in the INI (and be unable to overwrite them ;) ).

Source for the conversion tools is on Tursi's github. The open source packages are here:

ffmpeg - https://www.ffmpeg.org/
mogrify - http://www.imagemagick.org/
sox - http://sox.sourceforge.net/

For very long files, the rounding errors on the audio calculation may result in the audio losing sync. For that event, you can preserve the temp files, and then just manually run the last few steps with a higher playback frequency (more audio data) or lower (less) until you have an error of less than 1 frame.

To make that easier, here are all the steps taken in tividconvert.exe. Assume that your input file is named "VIDEO.AVI".
In the dircmd line, %d is replaced with the number of CPU cores in your system for parallel processing.
In the sox line, %d is replaced with the calculated audio frequency (this is the number you'd adjust). The calculation is ((Number Frames) * 1544 / (Duration in Seconds as a float)), round to integer.

md temp
TOOLS\ffmpeg -y -i VIDEO.AVI -f image2 -vf fps=8.6458 temp\scene%%05d.png
TOOLS\mogrify -verbose -resize 192x128^ -gravity Center -crop 192x128+0+0 +repage -gravity NorthWest -extent 256x192 temp\*.png
TOOLS\dircmd -%d temp\scene*.png Convert9918 "temp\$f" "temp\$f"
del /s temp\*.png
del /s temp\*.bmp
TOOLS\ffmpeg -y -i VIDEO.AVI -vn -ac 1 temp\test.wav
<read Duration from output of ffmpeg and calculate output frequency based on duration and number of frames written>
TOOLS\sox --temp temp\ --norm temp\test.wav -D -c 1 --rate %d -b 8 -e unsigned-integer temp\out.wav
TOOLS\audioconvert temp\out.wav temp\audio.bin
TOOLS\videopack temp\scene temp\audio.bin finalPACK.bin
TOOLS\cartrepack finalPACK.bin finalPACK8.bin
del testout.raw
<optionally "del /s temp", if you agree to>

---------------------------------------------
Changes:
---------------------------------------------
3/21/2015:
- Added proper support for non-widescreen aspect ratios. Everything should now properly scale to fit the screen (no more white bars on the right).
- Added F18A GPU enhanced playback. This is autodetected and (currently) can not be defeated (plan is for the later final version to have a key or config override). It is not yet tested on hardware, any feedback is helpful!
- Updates to cartrepack and both versions of the playback code to account for the above.

---------------------------------------------
3/22/2015:
- More fixes to cartrepack to make larger cartridges loop more cleanly

---------------------------------------------
2/5/2017:
- Quick fix to make videos loop cleanly rather than run off the end of the cart

---------------------------------------------
11/4/2018
- updated with new player that handles 512MB cartridges and plays back at the correct
rate even on hardware.

---------------------------------------------
5/7/2019
- source script updated with VideoDespeckle tool for final output
- You can also run VideoDespeckle on any cart binary you've previously created!

---------------------------------------------
7/20/2022
- supports double-rate mono videos
- passes default values to convert9918

