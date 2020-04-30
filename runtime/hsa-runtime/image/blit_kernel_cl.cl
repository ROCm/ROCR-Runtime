/// Kernel code for HSA image import/export/copy/clear in OpenCL C form.

__kernel void copy_image_to_buffer(
    __read_only image2d_array_t   src,
    __global    uint*       dstUInt,
    __global    ushort*     dstUShort,
    __global    uchar*      dstUChar,
    int4        srcOrigin,
    ulong4      dstOrigin,
    int4        size,
    uint4       format,
    ulong4      pitch)
{
    ulong    idxDst;
    int4     coordsSrc;
    uint4    texel;

    coordsSrc.x = get_global_id(0);
    coordsSrc.y = get_global_id(1);
    coordsSrc.z = get_global_id(2);
    coordsSrc.w = 0;

    if ((coordsSrc.x >= size.x) ||
        (coordsSrc.y >= size.y) ||
        (coordsSrc.z >= size.z)) {
        return;
    }

    idxDst = (coordsSrc.z * pitch.y + coordsSrc.y * pitch.x +
        coordsSrc.x) * format.z + dstOrigin.x;

    coordsSrc.x += srcOrigin.x;
    coordsSrc.y += srcOrigin.y;
    coordsSrc.z += srcOrigin.z;

    texel = read_imageui(src, coordsSrc);

    // Check components
    switch (format.x) {
    case 1:
        // Check size
        switch (format.y) {
        case 1:
            dstUChar[idxDst] = (uchar)texel.x;
            break;
        case 2:
            dstUShort[idxDst] = (ushort)texel.x;
            break;
        case 4:
            dstUInt[idxDst] = texel.x;
            break;
        }
    break;
    case 2:
        // Check size
        switch (format.y) {
        case 1:
            dstUShort[idxDst] = (ushort)texel.x |
               ((ushort)texel.y << 8);
            break;
        case 2:
            dstUInt[idxDst] = texel.x | (texel.y << 16);
            break;
        case 4:
            dstUInt[idxDst++] = texel.x;
            dstUInt[idxDst] = texel.y;
            break;
        }
    break;
    case 4:
        // Check size
        switch (format.y) {
        case 1:
            dstUInt[idxDst] = (uint)texel.x |
               (texel.y << 8) |
               (texel.z << 16) |
               (texel.w << 24);
            break;
        case 2:
            dstUInt[idxDst++] = texel.x | (texel.y << 16);
            dstUInt[idxDst] = texel.z | (texel.w << 16);
            break;
        case 4:
            dstUInt[idxDst++] = texel.x;
            dstUInt[idxDst++] = texel.y;
            dstUInt[idxDst++] = texel.z;
            dstUInt[idxDst] = texel.w;
            break;
        }
    break;
    }
}

__kernel void copy_buffer_to_image(
    __global    uint*       src,
    __write_only image2d_array_t  dst,
    ulong4      srcOrigin,
    int4        dstOrigin,
    int4        size,
    uint4       format,
    ulong4      pitch)
{
    ulong    idxSrc;
    int4     coordsDst;
    uint4    pixel;
    __global uint*   srcUInt = src;
    __global ushort* srcUShort = (__global ushort*)src;
    __global uchar*  srcUChar  = (__global uchar*)src;
    ushort   tmpUShort;
    uint     tmpUInt;

    coordsDst.x = get_global_id(0);
    coordsDst.y = get_global_id(1);
    coordsDst.z = get_global_id(2);
    coordsDst.w = 0;

    if ((coordsDst.x >= size.x) ||
        (coordsDst.y >= size.y) ||
        (coordsDst.z >= size.z)) {
        return;
    }

    idxSrc = (coordsDst.z * pitch.y +
       coordsDst.y * pitch.x + coordsDst.x) *
       format.z + srcOrigin.x;

    coordsDst.x += dstOrigin.x;
    coordsDst.y += dstOrigin.y;
    coordsDst.z += dstOrigin.z;

    // Check components
    switch (format.x) {
    case 1:
        // Check size
        if (format.y == 1) {
            pixel.x = (uint)srcUChar[idxSrc];
        }
        else if (format.y == 2) {
            pixel.x = (uint)srcUShort[idxSrc];
        }
        else {
            pixel.x = srcUInt[idxSrc];
        }
    break;
    case 2:
        // Check size
        if (format.y == 1) {
            tmpUShort = srcUShort[idxSrc];
            pixel.x = (uint)(tmpUShort & 0xff);
            pixel.y = (uint)(tmpUShort >> 8);
        }
        else if (format.y == 2) {
            tmpUInt = srcUInt[idxSrc];
            pixel.x = (tmpUInt & 0xffff);
            pixel.y = (tmpUInt >> 16);
        }
        else {
            pixel.x = srcUInt[idxSrc++];
            pixel.y = srcUInt[idxSrc];
        }
    break;
    case 4:
        // Check size
        if (format.y == 1) {
            tmpUInt = srcUInt[idxSrc];
            pixel.x = tmpUInt & 0xff;
            pixel.y = (tmpUInt >> 8) & 0xff;
            pixel.z = (tmpUInt >> 16) & 0xff;
            pixel.w = (tmpUInt >> 24) & 0xff;
        }
        else if (format.y == 2) {
            tmpUInt = srcUInt[idxSrc++];
            pixel.x = tmpUInt & 0xffff;
            pixel.y = (tmpUInt >> 16);
            tmpUInt = srcUInt[idxSrc];
            pixel.z = tmpUInt & 0xffff;
            pixel.w = (tmpUInt >> 16);
        }
        else {
            pixel.x = srcUInt[idxSrc++];
            pixel.y = srcUInt[idxSrc++];
            pixel.z = srcUInt[idxSrc++];
            pixel.w = srcUInt[idxSrc];
        }
    break;
    }
    // Write the final pixel
    write_imageui(dst, coordsDst, pixel);
}

__kernel void copy_image_default(
    __read_only  image2d_array_t src,
    __write_only image2d_array_t dst,
    int4    srcOrigin,
    int4    dstOrigin,
    int4    size)
{
    int4    coordsDst;
    int4    coordsSrc;

    coordsDst.x = get_global_id(0);
    coordsDst.y = get_global_id(1);
    coordsDst.z = get_global_id(2);
    coordsDst.w = 0;

    if ((coordsDst.x >= size.x) ||
        (coordsDst.y >= size.y) ||
        (coordsDst.z >= size.z)) {
        return;
    }

    coordsSrc = srcOrigin + coordsDst;
    coordsDst += dstOrigin;

    uint4  texel;
    texel = read_imageui(src, coordsSrc);
    write_imageui(dst, coordsDst, texel);
}

float linear_to_standard_rgba(float l_val) {
  float s_val = l_val;

  if (isnan(s_val)) s_val = 0.0f;

  if (s_val > 1.0f) {
    s_val = 1.0f;
  } else if (s_val < 0.0f) {
    s_val = 0.0f;
  } else if (s_val < 0.0031308f) {
    s_val = 12.92f * s_val;
  } else {
    s_val = (1.055f * pow(s_val, 5.0f / 12.0f)) - 0.055f;
  }

  return s_val;
}

__kernel void copy_image_linear_to_standard(
    __read_only  image2d_array_t src,
    __write_only image2d_array_t dst,
    int4    srcOrigin,
    int4    dstOrigin,
    int4    size,
    int     copyType)
{
    int4    coordsDst;
    int4    coordsSrc;

    coordsDst.x = get_global_id(0);
    coordsDst.y = get_global_id(1);
    coordsDst.z = get_global_id(2);
    coordsDst.w = 0;

    if ((coordsDst.x >= size.x) ||
        (coordsDst.y >= size.y) ||
        (coordsDst.z >= size.z)) {
        return;
    }

    coordsSrc = srcOrigin + coordsDst;
    coordsDst += dstOrigin;

    float4  texel;
    texel = read_imagef(src, coordsSrc);

    texel.x = linear_to_standard_rgba(texel.x);
    texel.y = linear_to_standard_rgba(texel.y);
    texel.z = linear_to_standard_rgba(texel.z);

    write_imagef(dst, coordsDst, texel);
}

__kernel void copy_image_standard_to_linear(
    __read_only  image2d_array_t src,
    __write_only image2d_array_t dst,
    int4    srcOrigin,
    int4    dstOrigin,
    int4    size,
    int     copyType)
{
    int4    coordsDst;
    int4    coordsSrc;

    coordsDst.x = get_global_id(0);
    coordsDst.y = get_global_id(1);
    coordsDst.z = get_global_id(2);
    coordsDst.w = 0;

    if ((coordsDst.x >= size.x) ||
        (coordsDst.y >= size.y) ||
        (coordsDst.z >= size.z)) {
        return;
    }

    coordsSrc = srcOrigin + coordsDst;
    coordsDst += dstOrigin;

    float4  texel;
    texel = read_imagef(src, coordsSrc);
    write_imagef(dst, coordsDst, texel);
}

__kernel void copy_image_1db(
    __read_only  image1d_buffer_t src,
    __write_only image1d_buffer_t dst,
    int4    srcOrigin,
    int4    dstOrigin,
    int4    size)
{
    int    coordDst;
    int    coordSrc;

    coordDst = get_global_id(0);

    if (coordDst >= size.x) {
        return;
    }

    coordSrc = srcOrigin.x + coordDst;
    coordDst += dstOrigin.x;

    uint4  texel;
    texel = read_imageui(src, coordSrc);
    write_imageui(dst, coordDst, texel);
}

__kernel void copy_image_1db_to_reg(
    __read_only  image1d_buffer_t src,
    __write_only image2d_array_t dst,
    int4    srcOrigin,
    int4    dstOrigin,
    int4    size)
{
    int4    coordsDst;
    int    coordSrc;

    coordsDst.x = get_global_id(0);
    coordsDst.y = get_global_id(1);
    coordsDst.z = get_global_id(2);
    coordsDst.w = 0;

    if (coordsDst.x >= size.x) {
        return;
    }

    coordSrc = srcOrigin.x + coordsDst.x;
    coordsDst += dstOrigin;

    uint4  texel;
    texel = read_imageui(src, coordSrc);
    write_imageui(dst, coordsDst, texel);
}

__kernel void copy_image_reg_to_1db(
    __read_only  image2d_array_t src,
    __write_only image1d_buffer_t dst,
    int4    srcOrigin,
    int4    dstOrigin,
    int4    size)
{
    int    coordDst;
    int4    coordsSrc;

    coordsSrc.x = get_global_id(0);
    coordsSrc.y = get_global_id(1);
    coordsSrc.z = get_global_id(2);
    coordsSrc.w = 0;

    if (coordsSrc.x >= size.x) {
        return;
    }

    coordDst = dstOrigin.x + coordsSrc.x;
    coordsSrc += srcOrigin;

    uint4  texel;
    texel = read_imageui(src, coordsSrc);
    write_imageui(dst, coordDst, texel);
}

__kernel void clear_image(
    __write_only image2d_array_t  image,
    float4     patternFLOAT4,
    int4       patternINT4,
    uint4      patternUINT4,
    int4       origin,
    int4       size,
    uint       type)
{
    int4  coords;

    coords.x = get_global_id(0);
    coords.y = get_global_id(1);
    coords.z = get_global_id(2);
    coords.w = 0;

    if ((coords.x >= size.x) ||
        (coords.y >= size.y) ||
        (coords.z >= size.z)) {
        return;
    }

    coords += origin;

    // Check components
    switch (type) {
    case 0:
        write_imagef(image, coords, patternFLOAT4);
        break;
    case 1:
        write_imagei(image, coords, patternINT4);
        break;
    case 2:
        write_imageui(image, coords, patternUINT4);
        break;
    }
}

__kernel void clear_image_1db(
    __write_only image1d_buffer_t  image,
    float4     patternFLOAT4,
    int4       patternINT4,
    uint4      patternUINT4,
    int4       origin,
    int4       size,
    uint       type)
{
    int coord = get_global_id(0);

    if (coord >= size.x) {
        return;
    }

    coord += origin.x;

    // Check components
    switch (type) {
    case 0:
        write_imagef(image, coord, patternFLOAT4);
        break;
    case 1:
        write_imagei(image, coord, patternINT4);
        break;
    case 2:
        write_imageui(image, coord, patternUINT4);
        break;
    }
}