#include "pca.h"
#include <iostream>
#include <iomanip>
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>

PCA::PCA(int width, int height, const std::vector<unsigned char>& imageData) : 
    width(width),
    height(height),
    imageData(imageData),
    processedImageData(imageData)
{
    if(this->imageData.size() % 4 != 0)
        throw std::runtime_error("only rgba8 format supported");
    /*this->blockSize = this->width * this->height / (8 * 8) * 4;*/
    /*this->blockSize = this->width * this->height / (1) * 4;*/

    this->setBlockSize(this->width * this->height);
    this->setGranularity(this->granularity);
}

void PCA::setBlockSize(size_t blockSize)
{
    constexpr size_t minBlockSize = 4;
    this->blockSize = std::clamp(
        blockSize * 4, 
        minBlockSize, 
        static_cast<size_t>(this->width * this->height * 4));
}

void PCA::setGranularity(int granularity)
{
    this->granularity = granularity;
    this->bitMask = 0;

    const int bitWidth = 32 / this->granularity;
    for(int i = 0; i < bitWidth; i++)
        this->bitMask = this->bitMask << 1 | 1;
}

bool PCA::processBlock(size_t blockIndex)
{
    if(this->blockSize * blockIndex >= this->imageData.size())
        return false;
    if(this->granularity < 1 || this->granularity > 32)
        return false;

    const int bitWidth = 32 / this->granularity;

    Eigen::MatrixXd dataMatrixAdjust;

    // read the rgba values of the image block
    for(size_t i = this->blockSize * blockIndex;
        i < std::min(this->blockSize * (blockIndex + 1), this->imageData.size());
        i += 4)
    {
        std::uint32_t pixel = *(std::uint32_t*)(this->imageData.data() + i);
        /*std::uint32_t pixel = this->imageData[i + 3];
        pixel = pixel << 8 | this->imageData[i + 2];
        pixel = pixel << 8 | this->imageData[i + 1];
        pixel = pixel << 8 | this->imageData[i];*/

        Eigen::VectorXd intensityVector;
        for(int j = 0; j < 32; j += bitWidth)
        {
            const auto rows = intensityVector.rows();
            intensityVector.conservativeResize(rows + 1, 1);
            intensityVector(rows) = pixel >> j & this->bitMask;
        }

        // add our rgba vector to new column in the dataMatrixAdjust
        const auto cols = dataMatrixAdjust.cols();
        // conservativeResize retains old data
        dataMatrixAdjust.conservativeResize(intensityVector.rows(), cols + 1);
        dataMatrixAdjust.col(cols) = intensityVector;
    }

    // for PCA to work, the data set has to be normalized by
    // subtracting the mean of the set.
    // this produces a data set whose mean is zero.
    Eigen::VectorXd dataMean;
    dataMean.resize(dataMatrixAdjust.rows(), 1);
    dataMean.setZero();
    for(auto i = 0; i < dataMatrixAdjust.cols(); i++)
        dataMean += dataMatrixAdjust.col(i);
    dataMean /= dataMatrixAdjust.cols();

    // subtract the mean from the data
    for(auto i = 0; i < dataMatrixAdjust.cols(); i++)
        dataMatrixAdjust.col(i) -= dataMean;

    // calculate the nxn covariance matrix.
    // the sample has n dimensions.
    Eigen::MatrixXd covarianceMatrix;
    covarianceMatrix.resize(dataMatrixAdjust.rows(), dataMatrixAdjust.rows());

    for(int i = 0; i < dataMatrixAdjust.rows(); i++)
    {
        for(int j = 0; j < dataMatrixAdjust.rows(); j++)
        {
            // calculate the covariance of the selected 2 dimensions (i and j)
            // from the dataMatrixAdjust;
            // the data set has the mean of each dimension already at zero, so 
            // no need to subtract it when calculating the covariance.
            double covariance = 0.0;
            for(auto k = 0; k < dataMatrixAdjust.cols(); k++)
                covariance += dataMatrixAdjust(i, k) * dataMatrixAdjust(j, k);
            covariance /= dataMatrixAdjust.cols() - 1;

            covarianceMatrix(i, j) = covariance;
        }
    }

    // calculate the eigenvectors and eigenvalues of the covariance matrix;
    // the eigenvectors are returned as unit vectors
    Eigen::EigenSolver<decltype(covarianceMatrix)> solver;
    solver.compute(covarianceMatrix, true);

    const auto& eigenValues = solver.eigenvalues();
    const auto& eigenVectors = solver.eigenvectors();

    /*for(auto i = 0; i < eigenVectors.cols(); i++)
    {
        std::cout << "eigenvector " << i << ":\n" << eigenVectors.col(i).real() << std::endl;
        std::cout << "eigenvalue " << i << ": " << eigenValues(i).real() << std::endl << std::endl;
    }*/

    // form a feature vector which has the eigenvectors we want to keep(3);
    // eigensolver returns the list of eigenvectors already sorted from the highest eigenvalue
    // to lowest;
    // we will make it a row feature vector so that it is easier to work with when
    // transforming the data back
    Eigen::MatrixXd featureVector = eigenVectors.real();
    // resize the feature vector
    featureVector.conservativeResize(
        Eigen::NoChange,
        std::min(this->quality, (int)featureVector.cols()));

    // form the final data in terms of the feature vectors;
    // the final data is something that can be compressed
    const auto rowFeatureVector = featureVector.transpose();
    const auto finalData = rowFeatureVector * dataMatrixAdjust;

    /*for(auto i = 0; i < finalData.cols(); i++)
    {
        std::cout << "final data:\n" << finalData.col(i) << std::endl;
        std::cout << "--------------------------------------------" << std::endl;
        system("pause");
    }*/

    // let's assume the data used to construct original data can be 
    // stored in 4 byte floating points
    this->compressedByteCount += dataMean.rows() * 4;
    this->compressedByteCount += rowFeatureVector.cols() * 4;
    // a single cell in the final data occupies bitWidth amount of bits
    const size_t bitCount = finalData.rows() * finalData.cols() * bitWidth;
    this->compressedByteCount += static_cast<size_t>(std::ceil(bitCount / 8.0));

    // get the old data back
    // (because the feature vector holds unit eigenvectors, then A^-1 = A^T)
    Eigen::MatrixXd oldData = rowFeatureVector.transpose() * finalData;
    for(auto i = 0; i < oldData.cols(); i++)
        oldData.col(i) += dataMean;

    /*for(auto i = 0; i < oldData.cols(); i++)
    {
        std::cout << "unprocessed data:\n" << dataMatrix.col(i) << "\n\n";
        std::cout << "processed data:\n" << oldData.col(i) << std::endl;
        std::cout << "--------------------------------------------" << std::endl;
        system("pause");
    }*/

    // write the processed image data
    for(auto i = 0; i < oldData.cols(); i++)
    {
        const auto startIndex = this->blockSize * blockIndex + i * 4;
        auto dataPtr = this->processedImageData.data() + startIndex;
        
        std::uint32_t pixel = 0;
        const int bitWidth = 32 / this->granularity;
        for(auto j = 0; j < oldData.rows(); j++)
        {
            pixel |= (std::uint32_t)
                std::clamp(std::round(oldData(j, i)), 0.0, (double)this->bitMask)
                << (bitWidth * j);
        }

        *(std::uint32_t*)dataPtr = pixel;

        /*dataPtr[0] = (unsigned char)std::clamp(oldData(0, i), 0.0, 255.0);
        dataPtr[1] = (unsigned char)std::clamp(oldData(1, i), 0.0, 255.0);
        dataPtr[2] = (unsigned char)std::clamp(oldData(2, i), 0.0, 255.0);
        dataPtr[3] = (unsigned char)std::clamp(oldData(3, i), 0.0, 255.0);*/
    }

    return true;
}

void PCA::process(int quality)
{
    this->quality = quality;
    this->compressedByteCount = 0;

    std::cout
        << "granularity: " << this->granularity
        << ", quality: " << this->quality
        << ", block size: " << this->blockSize;

    for(size_t i = 0; this->processBlock(i); i++);

    const size_t imageSizeWithoutAlphaChannel = this->imageData.size() - this->width * this->height;
    std::cout << ", compressed size: "
        << static_cast<int>(this->compressedByteCount * 100.0 / imageSizeWithoutAlphaChannel)
        << "%" << std::endl;
}