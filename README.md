# PCA demo

Simple lossy image compression using Principal Component Analysis. Inspiration coming from a great tutorial written by Lindsay I Smith called "A tutorial on Principal Components Analysis".

The pixel format of the raw image is 32 bit RGBA.

There are 3 parameters that can be used to control the image compression:

## Compression complexity or granularity
This selects how many "components" are there in a single pixel. With a granularity of 4, the 32 bit RGBA pixel is divided into four same-size components. So the first component is the least 8 bits of the 32 bit value, second component is the second least 8 bits of the 32 bits and so on. It turns out that for best results, granularity should be 4 so that each component is a single color channel of the 32 bit pixel.

## Compression level or quality
This selects how many components to keep in the compressed image. If quality=granularity, then no component is removed and there is no compression. The components removed are those with the least amount of information.

## Block size
Defines how many pixels are processed in a single principal component analysis. The pixels are processed line by line from left to right.
