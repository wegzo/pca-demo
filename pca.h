#pragma once

#include <vector>

class PCA
{
private:
    std::vector<unsigned char> imageData;
    std::vector<unsigned char> processedImageData;
    int width, height;
    size_t blockSize;
    int granularity = 4; // 1, 2, 4, 8, 16 or 32
    int quality = 4;
    std::uint32_t bitMask = 0;
    size_t compressedByteCount = 0;

    bool processBlock(size_t blockIndex);
public:
    // bit depth: 8, format: rgba
    explicit PCA(int width, int height,
        const std::vector<unsigned char>& imageData);

    void process(int quality);
    void setGranularity(int granularity);
    void setBlockSize(size_t blockSize);

    const std::vector<unsigned char>& getProcessedImageData() const { return this->processedImageData; }
};