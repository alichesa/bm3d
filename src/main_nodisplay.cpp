#include <iostream>
#include <string>
#include <vector>
#include "bm3d.hpp"
#define cimg_display 0
#include "CImg.h"
#include <limits>

using namespace cimg_library;

// 计算局部方差
double calculateVariance(const CImg<double>& block) {
    double mean = 0.0;
    double variance = 0.0;
    int totalPixels = block.width() * block.height();

    // 计算均值
    for (int i = 0; i < block.width(); ++i) {
        for (int j = 0; j < block.height(); ++j) {
            mean += block(i, j);
        }
    }
    mean /= totalPixels;

    // 计算方差
    for (int i = 0; i < block.width(); ++i) {
        for (int j = 0; j < block.height(); ++j) {
            variance += std::pow(block(i, j) - mean, 2);
        }
    }

    variance /= totalPixels;
    return variance;
}

// 估计噪声方差 sigma 并保存所有块的方差
double estimateNoiseStdAndSaveVariances(const CImg<unsigned char>& img, int blockSize, std::vector<double>& blockVariances) {
    int rows = img.height();
    int cols = img.width();

    double minVariance = std::numeric_limits<double>::max();
    CImg<double> block(blockSize, blockSize);
    double threshold = 0.1;

    // 清空并重置方差数组大小
    blockVariances.clear();
    blockVariances.resize(((rows + blockSize - 1) / blockSize) * ((cols + blockSize - 1) / blockSize), 0.0);
    int varianceIndex = 0;

    // 遍历图像，将其切割成小块
    for (int y = 0; y <= rows - blockSize; y += blockSize) {
        for (int x = 0; x <= cols - blockSize; x += blockSize) {
            // 提取一个小块
            for (int i = 0; i < blockSize; ++i) {
                for (int j = 0; j < blockSize; ++j) {
                    block(i, j) = static_cast<double>(img(x + j, y + i));
                }
            }

            // 计算小块的方差
            double variance = calculateVariance(block);
            
            // 保存方差到数组
            blockVariances[varianceIndex++] = variance;

            // 如果方差小于阈值，则认为该块是无噪声区域
            if (variance < minVariance && variance > threshold) {
                minVariance = variance;
            }
        }
    }

    if (minVariance == std::numeric_limits<double>::max()) {
        std::cerr << "No suitable block found for noise estimation!" << std::endl;
        return 0;
    }

    return std::sqrt(minVariance);
}

// 归一化方差
void normalizeVariances(std::vector<double>& blockVariances) {
    if (blockVariances.empty()) return;

    // 找到最大值和最小值
    double maxVariance = *std::max_element(blockVariances.begin(), blockVariances.end());
    double minVariance = *std::min_element(blockVariances.begin(), blockVariances.end());

    // 避免除以零
    if (maxVariance == minVariance) {
        std::cerr << "Error: All variances are identical!" << std::endl;
        return;
    }

    // 归一化每个方差值
    for (double& variance : blockVariances) {
        variance = (variance - minVariance) / (maxVariance - minVariance);
    }

    std::cout << "Variances normalized between 0 and 1." << std::endl;
}


int main(int argc, char** argv)
{
    float sigma = strtof(argv[3], NULL);

    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " NosiyImage DenoisedImage sigma [color [twostep [quiet [ReferenceImage]]]]" << std::endl;
        return 1;
    }

    unsigned int channels = 1;
    bool twostep = false;
    bool verbose = true;

    if (argc >= 5 && strcmp(argv[4], "color") == 0) {
        channels = 3;
    }
    if (argc >= 6 && strcmp(argv[5], "twostep") == 0) {
        twostep = true;
    }
    if (argc >= 7 && strcmp(argv[6], "quiet") == 0) {
        verbose = false;
    }

    CImg<unsigned char> image(argv[1]);
    CImg<unsigned char> image2(image.width(), image.height(), 1, channels, 0);

    int blockSize = 128;
    std::vector<double> blockVariances;

    // 一次性计算噪声标准差并保存所有块的方差
    double estimatedNoiseStd = estimateNoiseStdAndSaveVariances(image, blockSize, blockVariances);
    sigma = static_cast<float>(estimatedNoiseStd);

	normalizeVariances(blockVariances);


    if (verbose) {
        std::cout << "Sigma = " << sigma << std::endl;
        if (twostep)
            std::cout << "Number of Steps: 2" << std::endl;
        else
            std::cout << "Number of Steps: 1" << std::endl;

        if (channels > 1)
            std::cout << "Color denoising: yes" << std::endl;
        else
            std::cout << "Color denoising: no" << std::endl;
    }

    // 转换到YCbCr空间
    std::vector<unsigned int> sigma2(channels);
    sigma2[0] = (unsigned int)(sigma * sigma);
    
    if (channels == 3) {
        image = image.get_channels(0, 2).RGBtoYCbCr();
        long s = sigma * sigma;
        sigma2[0] = ((66l*66l*s + 129l*129l*s + 25l*25l*s) / (256l*256l));
        sigma2[1] = ((38l*38l*s + 74l*74l*s + 112l*112l*s) / (256l*256l));
        sigma2[2] = ((112l*112l*s + 94l*94l*s + 18l*18l*s) / (256l*256l));
    }

    std::cout << "Noise variance for individual channels (YCrCb if color): ";
    for (unsigned int k = 0; k < sigma2.size(); k++)
        std::cout << sigma2[k] << " ";
    std::cout << std::endl;

    if (!image.data()) {
        std::cerr << "Could not open or find the image" << std::endl;
        return 1;
    }

    if (verbose)
        std::cout << "width: " << image.width() << " height: " << image.height() << std::endl;

    // Launch BM3D
    try {
        BM3D bm3d;
        bm3d.set_hard_params(19, 8, 16, 2500, 3, 2.7f);
        bm3d.set_wien_params(19, 8, 32, 400, 3);
        bm3d.set_verbose(verbose);
        bm3d.denoise_host_image(
            image.data(),
            image2.data(),
            image.width(),
            image.height(),
            channels,
            sigma2.data(),
            twostep,
			blockVariances );
    }
    catch(std::exception & e) {
        std::cerr << "There was an error while processing image: " << std::endl << e.what() << std::endl;
        return 1;
    }
    
    if (channels == 3)
        image2 = image2.get_channels(0, 2).YCbCrtoRGB();
    else
        image2 = image2.get_channel(0);
        
    image2.save(argv[2]);

    if (argc >= 8) {
        CImg<unsigned char> reference_image(argv[7]);
        std::cout << "PSNR:" << reference_image.PSNR(image2) << std::endl;
    }

    return 0;
}