//#include "pca.h"
//#include <iostream>
//#include <Eigen/Dense>
//#include <Eigen/Eigenvalues>
//
//PCA::PCA(int width, int height, const std::vector<unsigned char>& imageData) :
//    width(width),
//    height(height),
//    imageData(imageData),
//    processedImageData(imageData)
//{
//    if(this->imageData.size() % 4 != 0)
//        throw std::runtime_error("only rgba8 format supported");
//    /*this->blockSize = this->width * this->height / (16 * 16) * 4;*/
//    this->blockSize = this->width * this->height / (1) * 4;
//}
//
//bool PCA::processBlock(size_t blockIndex)
//{
//    if(this->blockSize * blockIndex >= this->imageData.size())
//        return false;
//
//    Eigen::MatrixXd dataMatrix;
//
//    // read the rgba values of the image block
//    for(size_t i = this->blockSize * blockIndex;
//        i < std::min(this->blockSize * (blockIndex + 1), this->imageData.size());
//        i += 4)
//    {
//        Eigen::Vector4d rgba(
//            this->imageData[i],
//            this->imageData[i + 1],
//            this->imageData[i + 2],
//            this->imageData[i + 3]);
//
//        // add our rgba vector to new column in the dataMatrix
//        const auto cols = dataMatrix.cols();
//        dataMatrix.conservativeResize(4, cols + 1); // conservativeResize retains old data
//        dataMatrix.col(cols) = rgba;
//    }
//
//    // for PCA to work, the data set has to be normalized by
//    // subtracting the mean of the set.
//    // this produces a data set whose mean is zero.
//    Eigen::MatrixXd dataMatrixAdjust = dataMatrix;
//
//    Eigen::Vector4d dataMean(0.0, 0.0, 0.0, 0.0);
//    for(auto i = 0; i < dataMatrixAdjust.cols(); i++)
//        dataMean += dataMatrixAdjust.col(i);
//    dataMean /= dataMatrixAdjust.cols();
//
//    // subtract the mean from the data
//    for(auto i = 0; i < dataMatrixAdjust.cols(); i++)
//        dataMatrixAdjust.col(i) -= dataMean;
//
//    // calculate the 4x4 covariance matrix.
//    // the rgba sample has 4 dimensions.
//    Eigen::Matrix4d covarianceMatrix;
//
//    for(int i = 0; i < 4; i++)
//    {
//        for(int j = 0; j < 4; j++)
//        {
//            // calculate the covariance of the selected 2 dimensions (i and j)
//            // from the dataMatrixAdjust;
//            // the data set has the mean of each dimension already at zero, so 
//            // no need to subtract it when calculating the covariance.
//            double covariance = 0.0;
//            for(auto k = 0; k < dataMatrixAdjust.cols(); k++)
//                covariance += dataMatrixAdjust(i, k) * dataMatrixAdjust(j, k);
//            covariance /= dataMatrixAdjust.cols() - 1;
//
//            covarianceMatrix(i, j) = covariance;
//        }
//    }
//
//    // calculate the eigenvectors and eigenvalues of the covariance matrix;
//    // the eigenvectors are returned as unit vectors
//    Eigen::EigenSolver<decltype(covarianceMatrix)> solver;
//    solver.compute(covarianceMatrix, true);
//
//    const auto& eigenValues = solver.eigenvalues();
//    const auto& eigenVectors = solver.eigenvectors();
//
//    /*for(auto i = 0; i < eigenVectors.cols(); i++)
//    {
//        std::cout << "eigenvector " << i << ":\n" << eigenVectors.col(i).real() << std::endl;
//        std::cout << "eigenvalue " << i << ": " << eigenValues(i).real() << std::endl << std::endl;
//    }*/
//
//    // form a feature vector which has the eigenvectors we want to keep(3);
//    // eigensolver returns the list of eigenvectors already sorted from the highest eigenvalue
//    // to lowest;
//    // we will make it a row feature vector so that it is easier to work with when
//    // transforming the data back
//    Eigen::MatrixXd featureVector = eigenVectors.real();
//    // make a 4x3 matrix (leave out the least significant eigenvector)
//    featureVector.conservativeResize(4, 1);
//
//    // form the final data in terms of the feature vectors;
//    // the final data is something that can be compressed
//    const auto rowFeatureVector = featureVector.transpose();
//    const auto finalData = rowFeatureVector * dataMatrixAdjust;
//
//    /*for(auto i = 0; i < finalData.cols(); i++)
//    {
//        std::cout << "final data:\n" << finalData.col(i) << std::endl;
//        std::cout << "--------------------------------------------" << std::endl;
//        system("pause");
//    }*/
//
//    // get the old data back
//    // (because the feature vector holds unit eigenvectors, then A^-1 = A^T)
//    Eigen::MatrixXd oldData = rowFeatureVector.transpose() * finalData;
//    for(auto i = 0; i < oldData.cols(); i++)
//        oldData.col(i) += dataMean;
//
//    /*for(auto i = 0; i < oldData.cols(); i++)
//    {
//        std::cout << "unprocessed data:\n" << dataMatrix.col(i) << "\n\n";
//        std::cout << "processed data:\n" << oldData.col(i) << std::endl;
//        std::cout << "--------------------------------------------" << std::endl;
//        system("pause");
//    }*/
//
//    // write the processed image data
//    for(auto i = 0; i < oldData.cols(); i++)
//    {
//        const auto startIndex = this->blockSize * blockIndex + i * 4;
//        auto dataPtr = this->processedImageData.data() + startIndex;
//
//        dataPtr[0] = (unsigned char)std::clamp(oldData(0, i), 0.0, 255.0);
//        dataPtr[1] = (unsigned char)std::clamp(oldData(1, i), 0.0, 255.0);
//        dataPtr[2] = (unsigned char)std::clamp(oldData(2, i), 0.0, 255.0);
//        dataPtr[3] = (unsigned char)std::clamp(oldData(3, i), 0.0, 255.0);
//    }
//
//    return true;
//}
//
//void PCA::process()
//{
//    for(size_t i = 0; this->processBlock(i); i++);
//}