// The MIT License (MIT)
//
// Copyright (c) 2016 Northeastern University
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// Kernel Dimension Alternative Clustering (KDAC)
// Please refer to the paper published in PAMI by Liu, Dy and Jordan at:
// http://people.eecs.berkeley.edu/~jordan/papers/niu-dy-jordan-pami.pdf
// We try to follow naming conventions in the paper as much as possible.
// The lower cased variable names is the same as in the paper, and the
// upper cased matrix variable names in the paper are converted to lower
// case suffixed with "_matrix". For example:
// matrix U in the paper is named u_matrix in this implementation.

#ifndef CPP_INCLUDE_KDAC_H
#define CPP_INCLUDE_KDAC_H

#include <functional>
#include <vector>
#include <cmath>
#include "include/matrix.h"
#include "include/vector.h"
#include "include/cpu_operations.h"
#include "include/svd_solver.h"
#include "include/kmeans.h"
#include "include/spectral_clustering.h"
#include "Eigen/Core"
#include "include/util.h"
#include "include/kernel_types.h"


namespace Nice {
template<typename T>
class KDAC {
 public:
  /// This is the default constructor for KDAC
  /// Number of clusters c and reduced dimension q will be both set to 2
  KDAC():
    c_(2),
    q_(2),
    n_(0),
    d_(0),
    lambda_(1),
    alpha_(0.1),
    kernel_type_(kGaussianKernel),
    constant_(1.0),
    u_converge_(false),
    w_converge_(false),
    u_w_converge_(false),
    threshold_(0.01),
    x_matrix_(),
    w_matrix_(),
    pre_w_matrix_(),
    y_matrix_(),
    y_matrix_temp_(),
    y_matrix_tilde_(),
    d_matrix_(),
    d_matrix_to_the_minus_half_(),
    d_ii_(),
    d_i_(),
    didj_matrix_(),
    k_matrix_(),
    k_matrix_y_(),
    u_matrix_(),
    pre_u_matrix_(),
    u_matrix_normalized_(),
    l_matrix_(),
    h_matrix_(),
    gamma_matrix_(),
    a_matrix_list_(),
    clustering_result_()
  {}

  ~KDAC() {}
  KDAC(const KDAC& rhs) {}
  KDAC& operator=(const KDAC& rhs) {}

  /// Set the number of clusters c
  void SetC(int c) {
    c_ = c;
    CheckCQ();
  }

  /// Set the reduced dimension q
  void SetQ(int q) {
    q_ = q;
    CheckCQ();
  }

  Matrix<T> GetU(void) {
    return u_matrix_;
  }

  Matrix<T> GetW(void) {
    return w_matrix_;
  }

  Matrix<T> GetUNormalized(void) {
    return u_matrix_normalized_;
  }

  Matrix<T> GetL(void) {
    return l_matrix_;
  }

  Matrix<T> GetD(void) {
    return d_matrix_;
  }

  Matrix<T> GetDToTheMinusHalf(void) {
    return d_matrix_to_the_minus_half_;
  }

  Matrix<T> GetK(void) {
    return k_matrix_;
  }

  std::vector<Matrix<T>> GetAList(void) {
    return a_matrix_list_;
  }

  Matrix<T> GetYTilde(void) {
    return y_matrix_tilde_;
  }

  Matrix<T> GetGamma(void) {
    return gamma_matrix_;
  }

  /// Set the kernel type: kGaussianKernel, kPolynomialKernel, kLinearKernel
  /// And set the constant associated the kernel
  void SetKernel(KernelType kernel_type, float constant) {
    kernel_type_ = kernel_type;
    constant_ = constant;
  }

  Vector<T> GenOrthogonal(const Matrix<T> &plain,
                     const Vector<T> &vector) {
    Vector<T> projection = Vector<T>::Zero(plain.rows());
    for (int j = 0; j < plain.cols(); j++) {
      // projection = (v * u / u^2) * u
      projection += (vector * plain.col(j) /
          plain.col(j).squaredNorm()) * plain.col(j);
    }
    return vector - projection;
  }

  /// This function creates the first clustering result
  /// \param input_matrix
  /// The input matrix of n samples and d features where each row
  /// represents a sample
  /// \return
  /// It only generates the clustering result but does not returns it
  /// Users can use Predict() to get the clustering result returned
  void Fit(const Matrix<T> &input_matrix) {
    Init(input_matrix);
    // When there is no Y, it is the the first round when the second term
    // lambda * HSIC is zero, we do not need to optimize W, and we directly
    // go to kmeans where Y_0 is generated. And both u and v are converged.
    OptimizeU();
    RunKMeans();
  }

  /// This function creates an alternative clustering result
  /// Must be called after \ref Fit(const Matrix<T> &input_matrix)
  /// when the first clustering result is generated
  void Fit(void) {
    // Fit() without an input data matrix is only called when we already
    // have an existing clustering result in Y
    // When Y exist, we are generating an alternative view with a
    // given Y_previous by doing Optimize both W and U until they converge
    // Following the pseudo code in Algorithm 1 in the paper
    Init();
    while (!u_w_converge_) {
      pre_u_matrix_ = u_matrix_;
      pre_w_matrix_ = w_matrix_;
      OptimizeU();
      OptimizeW();
      u_w_converge_ = CheckConverged(u_matrix_, pre_u_matrix_, threshold_) &&
          CheckConverged(w_matrix_, pre_w_matrix_, threshold_);
    }
    RunKMeans();
  }

  /// Running Predict() after Fit() returns
  /// the current clustering result as a Vector of T
  /// \return
  /// A NICE vector of T that specifies the clustering result
  Vector<T> Predict(void) {
    if (clustering_result_.rows() == 0) {
      std::cerr << "Fit() must be run before Predict(), exiting" << std::endl;
      exit(1);
    } else {
      return clustering_result_;
    }
  }

 private:
  int c_;  // cluster number c
  int q_;  // reduced dimension q
  int n_;  // number of samples in input data X
  int d_;  // input data X dimension d
  float lambda_;  // Learning rate lambda
  float alpha_;  // Alpha in W optimization
  KernelType kernel_type_;  // The kernel type of the kernel matrix
  float constant_;  // In Gaussian kernel, this is sigma;
                    // In Polynomial kernel, this is the polynomial order
                    // In Linear kernel, this is c as well
  bool u_converge_;  // If matrix U reaches convergence, false by default
  bool w_converge_;  // If matrix W reaches convergence, false by default
  bool u_w_converge_;  // If matrix U and W both converge, false by default
  T threshold_;  // To determine convergence
  Matrix<T> x_matrix_;  // Input matrix X (n by d)
  Matrix<T> w_matrix_;  // Transformation matrix W (d by q). Initialized to I
  Matrix<T> pre_w_matrix_;  // W matrix from last iteration,
                            // to check convergence
  Matrix<T> y_matrix_;  // Labeling matrix Y (n by (c0 + c1 + c2 + ..))
  Matrix<T> y_matrix_temp_;  // The matrix that holds the current Y_i
  Matrix<T> y_matrix_tilde_;  // The kernel matrix for Y
  Matrix<T> d_matrix_;  // Diagonal degree matrix D (n by n)
  Matrix<T> d_matrix_to_the_minus_half_;  // D^(-1/2) matrix
  Vector<T> d_ii_;  // The diagonal vector of the matrix D
  Vector<T> d_i_;  // The diagonal vector of the matrix D^(-1/2)
  Matrix<T> didj_matrix_;  // The matrix whose element (i, j) equals to
                           // di * dj - the ith and jth element from vector d_i_
  Matrix<T> k_matrix_;  // Kernel matrix K (n by n)
  Matrix<T> k_matrix_y_;  // Kernel matrix for Y (n by n)
  Matrix<T> u_matrix_;  // Embedding matrix U (n by c)
  Matrix<T> pre_u_matrix_;  // The U from last iteration, to check convergence
  Matrix<T> u_matrix_normalized_;  // Row-wise normalized U
  Matrix<T> l_matrix_;  // D^(-1/2) * K * D^(-1/2)
  Matrix<T> h_matrix_;  // Centering matrix (n by n)
  Matrix<T> gamma_matrix_;  // The gamma matrix used in gamma_ij in formula 5
  std::vector<Matrix<T>> a_matrix_list_;  // An n*n list that contains all of
                                         // the A_ij matrix
  Vector<T> clustering_result_;  // Current clustering result


  // Initialization
  void Init(const Matrix<T> &input_matrix) {
    x_matrix_ = input_matrix;
    n_ = input_matrix.rows();
    d_ = input_matrix.cols();
    w_matrix_ = Matrix<T>::Identity(d_, d_);
    y_matrix_ = Matrix<T>::Zero(n_, c_);
    threshold_ = 0.01;
//    y_matrix_ = Matrix<bool>::Zero(n_, c_);
//    d_matrix_ = Matrix<T>::Zero(n_, n_, 0);
//    k_matrix_ = Matrix<T>::Zero(n_, n_, 0);
//    u_matrix_ = Matrix<T>::Zero(n_, c_, 0);
    h_matrix_ = Matrix<T>::Identity(n_, n_)
        - Matrix<T>::Constant(n_, n_, 1) / float(n_);
    y_matrix_temp_ = Matrix<T>::Zero(n_, c_);
    InitAMatrixList();
  }

  void Init(void) {
    u_converge_ = false;
    w_converge_ = false;
    u_w_converge_ = false;
  }

  void InitAMatrixList(void) {
    a_matrix_list_.resize(n_ * n_);
    for (int i = 0; i < n_; i++) {
      for (int j = 0; j < n_; j++) {
        Vector<T> delta_x_ij = x_matrix_.row(i) - x_matrix_.row(j);
//        Matrix<T> A_ij = delta_x_ij.transpose() * delta_x_ij;
        Matrix<T> a_ij = CpuOperations<T>::OuterProduct(delta_x_ij, delta_x_ij);
//        std::cout << A_ij << std::endl << std::endl;
        a_matrix_list_[i * n_ + j] = a_ij;
      }
    }
  }

  // Check if q is not bigger than c
  void CheckCQ() {
    if (q_ > c_) {
      std::cerr <<
          "Reduced dimension q cannot exceed cluster number c" << std::endl;
      exit(1);
    }
  }

  /// This function runs KMeans on the normalized U
  void RunKMeans() {
    KMeans<T> kms;
    clustering_result_ = kms.FitPredict(u_matrix_normalized_, c_);
    if (y_matrix_.cols() == c_) {
      // When this is calculating Y0
      for (int i = 0; i < n_; i++)
        y_matrix_(i, clustering_result_(i)) = 1;
    } else {
      // When this is to calculate Y_i and append it to Y_[0~i-1]
      for (int i = 0; i < n_; i++)
        y_matrix_temp_(i, clustering_result_(i)) = 1;
      Matrix<T> y_matrix_new(n_, y_matrix_.cols() + c_);
      y_matrix_new << y_matrix_, y_matrix_temp_;
      y_matrix_ = y_matrix_new;
      // Reset the y_matrix_temp holder to zero
      y_matrix_temp_.setZero();
    }
  }

  void UpdateGOfW(Matrix<T> &g_of_w,
                  const Vector<T>& w_l) {
    for (int i = 0; i < n_; i++) {
      for (int j = 0; j < n_; j++) {
        if (kernel_type_ == kGaussianKernel) {
//          std::cout << i << ", " << j << std::endl;
//          std::cout << g_of_w(i, j) *
            g_of_w(i, j) = g_of_w(i, j) *
                static_cast<T>(-w_l.transpose() * a_matrix_list_[i * n_ + j] *
                               w_l) / static_cast<T>(2 * pow(constant_, 2));
//            g_of_w_l = g_of_w_l / (2 * pow(constant_, 2));
//            g_of_w(i, j) = g_of_w(i, j) / (2 * pow(constant_, 2));
//          g_of_w(i, j) = g_of_w(i, j) *
//              exp( (-w_l.transpose() * a_matrix_list_[i * n_ + j] * w_l) /
//                   (2 * constant_ * constant_) );
        }
      }
    }
  }

  void CheckFinite(const Matrix<T> &matrix, std::string name) {
    if (!matrix.allFinite()) {
      std::cout << name << " not finite: " << std::endl << matrix << std::endl;
      exit(1);
    }
  }

  void CheckFinite(const Vector<T> &vector, std::string name) {
    if (!vector.allFinite()) {
      std::cout << name << ": " << std::endl << vector << std::endl;
      exit(1);
    }
  }

  Vector<T> GenWGradient(const Matrix<T> &g_of_w, const Vector<T> &w_l) {
    Vector<T> w_gradient = Vector<T>::Zero(d_);
    if (kernel_type_ == kGaussianKernel) {
      for (int i = 0; i < n_; i++) {
        for (int j = 0; j < n_; j++) {
          Matrix<T> &a_matrix_ij = a_matrix_list_[i * n_ + j];
          T exp_term = exp(static_cast<T>(-w_l.transpose() * a_matrix_ij * w_l)
                           / (2.0 * pow(constant_, 2)));
          w_gradient += -gamma_matrix_(i, j) * g_of_w(i, j) * exp_term *
              a_matrix_ij * w_l / pow(constant_, 2);
//          w_gradient += -gamma_matrix_(i, j) * g_of_w(i, j) *
//              exp( (-w_l.transpose() * a_matrix_ij * w_l) /
//                  (2 * pow(constant_, 2)) ) * a_matrix_ij * w_l;

        }
      }

    }
    return w_gradient;
  }

  void OptimizeU(void) {
    // Projects X to subspace W (n * d to n * q)
    // If this is the first round, then projected X equals to X
    Matrix<T> projected_x_matrix = x_matrix_ * w_matrix_;
    // Generate the kernel matrix based on kernel type from projected X
    k_matrix_ = CpuOperations<T>::GenKernelMatrix(
        projected_x_matrix, kernel_type_, constant_);
    // Generate degree matrix from the kernel matrix
    // d_i is the diagonal vector of degree matrix D

    // This is a reference to how to directly generate D^(-1/2)
    // Vector<T> d_i = k_matrix_.rowwise().sum().array().sqrt().unaryExpr(
    //     std::ptr_fun(util::reciprocal<T>));
    // d_matrix_ = d_i.asDiagonal();

    // Generate D and D^(-1/2)
    GenDegreeMatrix();
    l_matrix_ = d_matrix_to_the_minus_half_ * k_matrix_ *
        d_matrix_to_the_minus_half_;
    SvdSolver<T> solver;
    solver.Compute(l_matrix_);
    // Generate a u matrix from SVD solver and then use Normalize to normalize
    // its rows
    u_matrix_ = solver.MatrixU().leftCols(c_);
    u_matrix_normalized_ = CpuOperations<T>::Normalize(u_matrix_, 2, 1);

  }

  void OptimizeW(void) {
    // Initialize lambda
    lambda_ = 0;
    // Generate the kernel for the label matrix Y: K_y
    k_matrix_y_ = y_matrix_ * y_matrix_.transpose();
    // Generate Y tilde matrix in equation 5 from kernel matrix of Y
    y_matrix_tilde_ = h_matrix_ * k_matrix_y_ * h_matrix_;

    // didj matrix contains the element (i, j) that equal to d_i * d_j
    didj_matrix_ = d_i_ * d_i_.transpose();

    // Generate the Gamma matrix in equation 5, which is a constant since
    // we have U fixed. Note that instead of generating one element of
    // gamma_ij on the fly as in the paper, we generate the whole gamma matrix
    // at one time and then access its entry of (i, j)
    // This is an element-wise operation
    // u*ut and didj matrix has the same size

    gamma_matrix_ = ((u_matrix_ * u_matrix_.transpose()).array() /
        didj_matrix_.array()).matrix() - lambda_ * y_matrix_tilde_;
    CheckFinite(gamma_matrix_, "gamma_matrix after");

    // After gamma_matrix is generated, we are optimizing gamma * kij as in 5
    // g_of_w is g(w_l) that is multiplied by g(w_(l+1)) in each iteration
    // of changing l.
    // Note that here the g_of_w is a n*n matrix because it contains A_ij
    // g_of_w(i, j) corresponding to exp(-w_T * A_ij * w / 2sigma^2)
    // When l = 0, g_of_w is 1
    // when l = 1, g_of_w is 1 .* g(w_1)
    // when l = 2, g_of_w is 1 .* g(w_1) .* g(w_2)...
    Matrix<T> g_of_w = Matrix<T>::Constant(n_, n_, 1);

    // We optimize each column in the W matrix
    for (int l = 0; l < w_matrix_.cols(); l++) {
      std::cout.precision(8);
      std::cout << std::scientific;
//      std::cout << l << "th column:" << std::endl;

      // Optimize the column vector in w_matrix w_l
      Vector<T> w_l;
      // Get orthogonal to make w_l orthogonal to vectors from w_0 to w_(l-1)
      // when l is not 0
      if (l == 0)
        w_l = w_matrix_.col(l);
      else
        w_l = GenOrthogonal(w_matrix_.leftCols(l), w_matrix_.col(l));
      // Normalize w_l
      w_l = w_l.array() / w_l.norm();
      // Search for the w_l that maximizes formula 5
      bool w_l_converged = false;
      int num_iter = 0;
      while (!w_l_converged) {
        Vector<T> w_l_gradient_vertical;
        Vector<T> pre_w_l = w_l;
        // Calculate the w gradient in equation 13
        Vector<T> w_l_gradient = GenWGradient(g_of_w, w_l);
//        std::cout << w_l_gradient << std::endl;
        CheckFinite(w_l_gradient, "w_l_gradient");
//        std::cout << w_l_gradient(0) << "\t" << w_l_gradient(1) << std::endl;
        // If this is the first column w_0, then we use the gradient directly
        // for updating
        if (l == 0) {
//          std::cout << "grad: " <<
//              w_l_gradient(0) << "\t" << w_l_gradient(1) << std::endl;
          alpha_ = LineSearch(w_l, w_l_gradient);
          std::cout << "alpha: " << alpha_ << std::endl; 
          w_l = sqrt(1.0 - pow(alpha_, 2)) * w_l + alpha_ * w_l_gradient;
          std::cout << "w_l: " << w_l(0) << "\t" << w_l(1) << std::endl;
//          std::cout << std::endl << std::endl;
        }
        // If this is a column from w_1 to w_d, then we need to use the
        // gradient that is vertical to the space spanned by w_0 to w_l
        else {
          w_l_gradient_vertical =
              GenOrthogonal(w_matrix_.leftCols(l), w_l_gradient);
          // Make w_gradient norm 1
          w_l_gradient_vertical = w_l_gradient_vertical.array() /
              w_l_gradient_vertical.norm();
          alpha_ = LineSearch(w_l, w_l_gradient_vertical);
          w_l = sqrt(1.0 - pow(alpha_, 2)) * w_l +
              alpha_ * w_l_gradient_vertical;
        }
        num_iter++;
        std::cout << "The " << num_iter << "th iteration: " << std::endl;
//          std::cout << "w_l:\n" << w_l << std::endl;
//        std::cout << w_l_gradient(0) << "\t" << w_l_gradient(1) << std::endl;
//          std::cout << "vertical:\n" << w_l_gradient_vertical << std::endl;
        if (num_iter > 1000)
          exit(1);
        CheckFinite(w_l, "w_l");
        w_l_converged = CheckConverged(w_l, pre_w_l, threshold_);
//        if (num_iter > 990) {
//          for (int i = 0; i < d_; i++)
//            std::cout << pre_w_l(i) << " \t " << w_l(i) << " \t" <<
//            w_l_gradient_vertical(i) << std::endl;
//        }
        if (w_l_converged)
          std::cout << "Converged" << std::endl;
        w_matrix_.col(l) = w_l;
      }
      // Update col l in matrix w by the new w_l
      // TODO: Need to learn about if using Vector<T> &w_l = w_matrix_.col(l)
      // would be better
      UpdateGOfW(g_of_w, w_l);
    }
  }

  float LineSearch(const Vector<T> &w_l, const Vector<T> &gradient) {
    float alpha = 1.0;
    float a1 = 0.1;
    float rho = 0.8;


    float phi_of_alpha = 0;
    float phi_of_alpha_prime = 0;
    float phi_of_zero = 0;
    float phi_of_zero_prime = 0;

    // Three terms used to calculate phi of alpha
    // They only change if w_l or gradient change
    Matrix<T> waw_matrix(n_, n_);
    Matrix<T> waf_matrix(n_, n_);
    Matrix<T> faf_matrix(n_, n_);
    GenPhiCoeff(w_l, gradient,
                &waw_matrix, &waf_matrix, &faf_matrix);
    GenPhi(alpha, waw_matrix, waf_matrix, faf_matrix, false,
              &phi_of_alpha, &phi_of_alpha_prime);
    GenPhi(0, waw_matrix, waf_matrix, faf_matrix, true,
              &phi_of_zero, &phi_of_zero_prime);
//    std::cout << "alpha: " << alpha << std::endl;
//    std::cout << "phi zero prime: " << phi_of_zero_prime << std::endl;
    if (phi_of_zero_prime < 0) {
      std::cout << "phi less than zero";
      exit(1);
    }
//    std::cout << "old phi: " << phi_of_zero << std::endl;
    while (phi_of_alpha < phi_of_zero + alpha * a1 * phi_of_zero_prime) {
      alpha = alpha * rho;
      GenPhi(alpha, waw_matrix, waf_matrix, faf_matrix, false,
                &phi_of_alpha, &phi_of_alpha_prime);
    }
    std::cout << "obj: " << phi_of_alpha << std::endl;
    return alpha;
  }

  void GenPhiCoeff(const Vector<T> &w_l, const Vector<T> &gradient,
                   Matrix<T> *waw, Matrix<T> *waf, Matrix<T> *faf) {
    if (kernel_type_ == kGaussianKernel) {
      for (int i = 0; i < n_; i++) {
        for (int j = 0; j < n_; j++) {
          Matrix<T> &a_matrix_ij = a_matrix_list_[i * n_ + j];
          (*waw)(i, j) = w_l.transpose() * a_matrix_ij * w_l;
          (*waf)(i, j) = w_l.transpose() * a_matrix_ij * gradient;
          (*faf)(i, j) = gradient.transpose() * a_matrix_ij * gradient;
        }
      }
    }
  }

  void GenPhi(const float &alpha,
               const Matrix<T> &waw,
               const Matrix<T> &waf,
               const Matrix<T> &faf,
               const bool &gen_prime,
               float *phi_of_alpha,
               float *phi_of_alpha_prime
               ) {

    if (kernel_type_ == kGaussianKernel) {
      // Generate phi of alpha, if gen_prime is true, the prime of phi of alpha
      // will also be generated
      Matrix<T> k_matrix_ij_alpha(n_, n_);
      Matrix<T> k_matrix_ij_alpha_prime(n_, n_);

      *phi_of_alpha = 0;
      *phi_of_alpha_prime = 0;

      GenKij(alpha, waw, waf, faf, gen_prime,
             &k_matrix_ij_alpha, &k_matrix_ij_alpha_prime);
      for (int i = 0; i < n_; i++) {
        for (int j = 0; j < n_; j++) {
          *phi_of_alpha += gamma_matrix_(i, j) * k_matrix_ij_alpha(i, j);
          *phi_of_alpha_prime += gamma_matrix_(i, j) *
              k_matrix_ij_alpha_prime(i, j);
        }
      }
    }
  }

  // This k_matrix_ij is from the univariate function phi from the formula after 8
  // in the paper. Note that this Kij is not the kernel function for optimizing
  // W as in 5, because we need to use dimension growth algorithm to optimize
  // W which would break Kij
  // a to e is the abbreviation of terms in the formula of phi(alpha) that
  // I deducted. I use those to make the code more readable. But I do not
  // know how to write the formula down here in the comment section to
  // make the term more understandable. Maybe I could upload a picture
  // somewhere?
  void GenKij(const float &alpha,
              const Matrix<T> &waw_matrix,
              const Matrix<T> &waf_matrix,
              const Matrix<T> &faf_matrix,
              const bool &gen_prime,
              Matrix<T> *k_matrix_ij,
              Matrix<T> *k_matrix_ij_prime) {
    if (kernel_type_ == kGaussianKernel) {
      float alpha_square = pow(alpha, 2);
      float sqrt_one_minus_alpha = pow((1-alpha_square), 0.5);
      float denom = -1 / (2 * pow(constant_, 2));
      for (int i = 0; i < n_; i++) {
        for (int j = 0; j < n_; j++) {
          float waw = waw_matrix(i, j);
          float waf = waf_matrix(i, j);
          float faf = faf_matrix(i, j);
          (*k_matrix_ij)(i, j) = exp(
              denom *
              (
                    (faf - waw) * alpha_square +
                    2 * waf * sqrt_one_minus_alpha * alpha +
                    waw
              )
                   );
          if (gen_prime) {
            (*k_matrix_ij_prime)(i, j) = denom *
                (2 * waf * (1 - 2 * alpha_square) / sqrt_one_minus_alpha +
                2 * (faf - waw) * alpha)
                    * (*k_matrix_ij)(i, j);
          }
        }
      }
    }
  }



  /// Generates a degree matrix D from an input kernel matrix
  /// It also generates D^(-1/2) and two diagonal vectors
  void GenDegreeMatrix(void) {
    // Generate the diagonal vector d_i and degree matrix D
    d_ii_ = k_matrix_.rowwise().sum();
    d_matrix_ = d_ii_.asDiagonal();
    // Generate matrix D^(-1/2)
    d_i_ = d_ii_.array().sqrt().unaryExpr(std::ptr_fun(util::reciprocal<T>));
    d_matrix_to_the_minus_half_ = d_i_.asDiagonal();
  }



  bool CheckConverged(const Matrix<T> &matrix, Matrix<T> &pre_matrix,
                     const T &threshold) {
//    // If this is the first time the matrix is generated, it is not converged
//    if (matrix.rows() == 0) {
//      pre_matrix = matrix;
//      return false;
//      // else check if the change is less than the threshold
//    } else {
    T change = CpuOperations<T>::FrobeniusNorm(matrix - pre_matrix)
          / CpuOperations<T>::FrobeniusNorm(pre_matrix);
    bool converged = (change < threshold);
    return converged;
//    }
  }

  bool CheckConverged(const Vector<T> &vector, Vector<T> &pre_vector,
                      const T &threshold) {
//    // If this is the first time the vecotr is generated, it is not converged
//    if (vector.size() == 0) {
//      pre_vector = vector;
//      return false;
//      // else check if the change is less than the threshold
//    } else {
//      std::cout << "pre_w_l: \n" << pre_vector(0) << ", " << pre_vector(1) << std::endl;
//      std::cout << "cur_w_l: \n" << vector(0) << ", " << vector(1) << std::endl;
//      std::cout << "diff: " << (vector - pre_vector).norm() << std::endl;
//      std::cout << "pre_vector norm: " << pre_vector.norm() << std::endl;
//      std::cout << "cur_vector norm: " << vector.norm() << std::endl;
      T change = (vector - pre_vector).norm() / pre_vector.norm();
//      std::cout << "change: " << change << std::endl;
      bool converged = (change < threshold);
//      std::cout << change << std::endl;
      return converged;
//    }
  }

};
}  // namespace NICE

#endif  // CPP_INCLUDE_KDAC_H
