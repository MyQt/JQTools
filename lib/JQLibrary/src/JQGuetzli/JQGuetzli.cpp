﻿#include "JQGuetzli.h"

// Qt lib import
#include <QDebug>
#include <QFileInfo>
#include <QTime>

// guetzli lib import
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <string>
#include <string.h>
#include "gflags/gflags.h"
#include "png/png.h"
#include "guetzli/processor.h"
#include "guetzli/quality.h"
#include "guetzli/stats.h"

// Workaround for differences between versions of gflags.
namespace gflags {
}
using namespace gflags;
namespace google {
}
using namespace google;


DEFINE_bool(verbose, false,
            "Print a verbose trace of all attempts to standard output.");
DEFINE_double(quality, 95,
              "Visual quality to aim for, expressed as a JPEG quality value.");

inline uint8_t JQGuetzli_BlendOnBlack(const uint8_t val, const uint8_t alpha) {
    return (static_cast<int>(val) * static_cast<int>(alpha) + 128) / 255;
}

bool JQGuetzli_ReadPNG(FILE* f, int* xsize, int* ysize,
             std::vector<uint8_t>* rgb) {
    png_structp png_ptr =
            png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        return false;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        return false;
    }

    if (setjmp(png_jmpbuf(png_ptr)) != 0) {
        // Ok we are here because of the setjmp.
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        return false;
    }

    rewind(f);
    png_init_io(png_ptr, f);

    // The png_transforms flags are as follows:
    // packing == convert 1,2,4 bit images,
    // strip == 16 -> 8 bits / channel,
    // shift == use sBIT dynamics, and
    // expand == palettes -> rgb, grayscale -> 8 bit images, tRNS -> alpha.
    const unsigned int png_transforms =
            PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_STRIP_16;

    png_read_png(png_ptr, info_ptr, png_transforms, nullptr);

    png_bytep* row_pointers = png_get_rows(png_ptr, info_ptr);

    *xsize = png_get_image_width(png_ptr, info_ptr);
    *ysize = png_get_image_height(png_ptr, info_ptr);
    rgb->resize(3 * (*xsize) * (*ysize));

    const int components = png_get_channels(png_ptr, info_ptr);
    switch (components) {
    case 1: {
        // GRAYSCALE
        for (int y = 0; y < *ysize; ++y) {
            const uint8_t* row_in = row_pointers[y];
            uint8_t* row_out = &(*rgb)[3 * y * (*xsize)];
            for (int x = 0; x < *xsize; ++x) {
                const uint8_t gray = row_in[x];
                row_out[3 * x + 0] = gray;
                row_out[3 * x + 1] = gray;
                row_out[3 * x + 2] = gray;
            }
        }
        break;
    }
    case 2: {
        // GRAYSCALE + ALPHA
        for (int y = 0; y < *ysize; ++y) {
            const uint8_t* row_in = row_pointers[y];
            uint8_t* row_out = &(*rgb)[3 * y * (*xsize)];
            for (int x = 0; x < *xsize; ++x) {
                const uint8_t gray = JQGuetzli_BlendOnBlack(row_in[2 * x], row_in[2 * x + 1]);
                row_out[3 * x + 0] = gray;
                row_out[3 * x + 1] = gray;
                row_out[3 * x + 2] = gray;
            }
        }
        break;
    }
    case 3: {
        // RGB
        for (int y = 0; y < *ysize; ++y) {
            const uint8_t* row_in = row_pointers[y];
            uint8_t* row_out = &(*rgb)[3 * y * (*xsize)];
            memcpy(row_out, row_in, 3 * (*xsize));
        }
        break;
    }
    case 4: {
        // RGBA
        for (int y = 0; y < *ysize; ++y) {
            const uint8_t* row_in = row_pointers[y];
            uint8_t* row_out = &(*rgb)[3 * y * (*xsize)];
            for (int x = 0; x < *xsize; ++x) {
                const uint8_t alpha = row_in[4 * x + 3];
                row_out[3 * x + 0] = JQGuetzli_BlendOnBlack(row_in[4 * x + 0], alpha);
                row_out[3 * x + 1] = JQGuetzli_BlendOnBlack(row_in[4 * x + 1], alpha);
                row_out[3 * x + 2] = JQGuetzli_BlendOnBlack(row_in[4 * x + 2], alpha);
            }
        }
        break;
    }
    default:
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        return false;
    }
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    return true;
}

std::string JQGuetzli_ReadFile(FILE* f) {
    if (fseek(f, 0, SEEK_END) != 0) {
        perror("fseek");
        return { };
    }
    off_t size = ftell(f);
    if (size < 0) {
        perror("ftell");
        return { };
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        perror("fseek");
        return { };
    }
    std::unique_ptr<char[]> buf(new char[size]);
    if (fread(buf.get(), 1, size, f) != (size_t)size) {
        perror("fread");
        return { };
    }
    std::string result(buf.get(), size);
    return result;
}

bool JQGuetzli_WriteFile(FILE* f, const std::string& contents) {
    if (fwrite(contents.data(), 1, contents.size(), f) != contents.size()) {
        perror("fwrite");
        return false;
    }
    if (fclose(f) < 0) {
        perror("fclose");
        return false;
    }

    return true;
}

JQGuetzli::ProcessResult JQGuetzli::process(const QString &inputImageFilePath, const QString &outputImageFilePath)
{
    QTime time;
    time.start();

    ProcessResult result;

    result.originalSize = QFileInfo( inputImageFilePath ).size();

    FILE* fin = fopen(inputImageFilePath.toLocal8Bit().data(), "rb");
    if (!fin) {
        fprintf(stderr, "Can't open input file\n");
        return result;
    }

    std::string in_data = JQGuetzli_ReadFile(fin);
    std::string out_data;

    if ( in_data.empty() )
    {
        return result;
    }

    guetzli::Params params;
    params.butteraugli_target =
            guetzli::ButteraugliScoreForQuality(FLAGS_quality);

    guetzli::ProcessStats stats;

    if (FLAGS_verbose) {
        stats.debug_output_file = stdout;
    }

    static const unsigned char kPNGMagicBytes[] = {
        0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n',
    };
    if (in_data.size() >= 8 &&
            memcmp(in_data.data(), kPNGMagicBytes, sizeof(kPNGMagicBytes)) == 0) {
        int xsize, ysize;
        std::vector<uint8_t> rgb;
        if (!JQGuetzli_ReadPNG(fin, &xsize, &ysize, &rgb)) {
            fprintf(stderr, "Error reading PNG data from input file\n");
            return result;
        }
        if (!guetzli::Process(params, &stats, rgb, xsize, ysize, &out_data)) {
            fprintf(stderr, "Guetzli processing failed\n");
            return result;
        }
    } else {
        if (!guetzli::Process(params, &stats, in_data, &out_data)) {
            fprintf(stderr, "Guetzli processing failed\n");
            return result;
        }
    }

    fclose(fin);

    FILE* fout = fopen(outputImageFilePath.toLocal8Bit().data(), "wb");
    if (!fout) {
        fprintf(stderr, "Can't open output file for writing\n");
        return result;
    }

    if ( !JQGuetzli_WriteFile(fout, out_data) )
    {
        return result;
    }

    result.processSucceed = true;
    result.resultSize = QFileInfo( outputImageFilePath ).size();
    result.compressionRatio = (double)result.resultSize / (double)result.originalSize;
    result.timeConsuming = time.elapsed();

    return result;
}
