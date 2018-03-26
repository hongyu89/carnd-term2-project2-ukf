#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 * This is scaffolding, do not modify
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 9.0;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 30.0;
  
  //DO NOT MODIFY measurement noise values below these are provided by the sensor manufacturer.
  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  //DO NOT MODIFY measurement noise values above these are provided by the sensor manufacturer.
  
  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */

  ///* State dimension
  n_x_ = 5;

  ///* Augmented state dimension
  n_aug_ = 7;

  ///* Weights of sigma points
  weights_ = VectorXd(2*n_aug_+1);
  double w = lambda_/(lambda_ + n_aug_);
  weights_(0) = w;
  for (int i=1; i<2*n_aug_+1; i++) {
    weights_(i) = 0.5/(lambda_ + n_aug_);
  }

  ///* Sigma point spreading parameter
  lambda_ = 3 - n_aug_;
  is_initialized_ = false;
  Xsig_pred_ = MatrixXd(n_x_, 2*n_aug_ + 1);
}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */
  if (!is_initialized_) {
    // initialize the state using first measurement
    cout << "UKF: " << endl;
    //x_ = VectorXd(5);
    //x_ << 1, 1, 1, 1, 1; // px, py, v, phi, phi_dot

    if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
      /**
       * convert radar from polar to cartesian coordinates and initialize states
       */

      float rho = meas_package.raw_measurements_[0];
      float phi = meas_package.raw_measurements_[1];
      float rho_dot = meas_package.raw_measurements_[2];
      x_ << rho * cos(phi), rho * sin(phi), 0, 0, 0; //px, py, v, phi, phi_dot
      P_<<std_radr_*std_radr_,0,0,0,0,
              0,std_radr_*std_radr_,0,0,0,
              0,0,std_radrd_*std_radrd_,0,0,
              0,0,0,std_radphi_*std_radphi_,0,
              0,0,0,0,1;
    }
    else if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
      /**
       * Initialize state.
       */

      float px = meas_package.raw_measurements_[0];
      float py = meas_package.raw_measurements_[1];
        x_ << px, py, 0.0, 0.0, 0.0; // px, py, v, phi, phi_dot
      P_<<std_laspx_*std_laspx_,0,0,0,0,
              0,std_laspy_*std_laspy_,0,0,0,
              0,0,1,0,0,
              0,0,0,1,0,
              0,0,0,0,1;
    }
    cout << "UKF initialization finished" << endl;
    is_initialized_ = true;
    time_us_ = meas_package.timestamp_;
    return;
  }

  double dt = (meas_package.timestamp_ - time_us_)/1000000.0;
    time_us_ = meas_package.timestamp_;

  if (use_laser_ && meas_package.sensor_type_ == MeasurementPackage::LASER) {
    Prediction(dt);
    UpdateLidar(meas_package);
    //time_us_ = meas_package.timestamp_;
  }
  if(use_radar_ && meas_package.sensor_type_ == MeasurementPackage::RADAR) {
    Prediction(dt);
    UpdateRadar(meas_package);
    //time_us_ = meas_package.timestamp_;
  }
}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */

  VectorXd x_aug = VectorXd(n_aug_);
  MatrixXd p_aug = MatrixXd(n_aug_, n_aug_);

  // augment state x_
  p_aug.fill(0.0);
  x_aug.head(n_x_) = x_;
  x_aug(5) = 0.0;
  x_aug(6) = 0.0;

  p_aug.topLeftCorner(n_x_, n_x_) = P_;
  p_aug(5, 5) = std_a_ * std_a_;
  p_aug(6, 6) = std_yawdd_ * std_yawdd_;

  MatrixXd A = p_aug.llt().matrixL();
  MatrixXd xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);
  xsig_aug.col(0) = x_aug;
  for (int i = 0; i < n_aug_; i++) {
    xsig_aug.col(i + 1) = x_aug + sqrt(lambda_ + n_aug_) * A.col(i);
    xsig_aug.col(i + 1 + n_aug_) = x_aug - sqrt(lambda_ + n_aug_) * A.col(i);
  }

  // predict sigma points
  VectorXd state = VectorXd(n_x_);
  VectorXd cov = VectorXd(n_x_);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    state.fill(0.0);
    x_aug = xsig_aug.col(i);
    cov << 0.5 * delta_t * delta_t * cos(x_aug(3)) * x_aug(5),
            0.5 * delta_t * delta_t * sin(x_aug(3)) * x_aug(5),
            delta_t * x_aug(5),
            0.5 * delta_t * delta_t * x_aug(6),
            delta_t * x_aug(6);
    if (x_aug(4) == 0) {
      state(0) = x_aug(2) * cos(x_aug(3)) * delta_t;
      state(1) = x_aug(2) * sin(x_aug(3)) * delta_t;
    } else {
      state(0) = x_aug(2) / x_aug(4) * (sin(x_aug(3) + x_aug(4) * delta_t) - sin(x_aug(3)));
      state(1) = x_aug(2) / x_aug(4) * (-cos(x_aug(3) + x_aug(4) * delta_t) + cos(x_aug(3)));
      state(3) = delta_t * x_aug(4);
    }
    Xsig_pred_.col(i) = x_aug.head(n_x_) + state + cov;

  }

  // predit mean and covariance
  x_ = Xsig_pred_ * weights_;
    while (x_(3) > M_PI) { x_(3) -= 2 * M_PI; }
    while (x_(3) < -M_PI) { x_(3) += 2 * M_PI; }
  P_.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    while (x_diff(3) > M_PI) { x_diff(3) -= 2 * M_PI; }
    while (x_diff(3) < -M_PI) { x_diff(3) += 2 * M_PI; }
    P_ = P_ + weights_(i) * x_diff * x_diff.transpose();
  }
}


/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */
  int n_z=2;
  VectorXd z_m = VectorXd(2);
  z_m<<meas_package.raw_measurements_(0),meas_package.raw_measurements_(1);
  MatrixXd zp_sig = MatrixXd(n_z,2*n_aug_+1);
  VectorXd zp_mean = VectorXd(n_z);
  MatrixXd zp_cov = MatrixXd(n_z,n_z);
  zp_sig.fill(0.0);
  zp_cov.fill(0.0);
  for(int i=0;i<2*n_aug_+1;i++) {
    zp_sig(0,i) = Xsig_pred_(0,i);
    zp_sig(1,i) = Xsig_pred_(1,i);
  }
  zp_mean=zp_sig*weights_;
  for(int i=0;i<2*n_aug_+1;i++) {
    VectorXd z_diff = zp_sig.col(i)-zp_mean;
    zp_cov = zp_cov+weights_(i)*z_diff*z_diff.transpose();
  }
  MatrixXd R = MatrixXd(n_z,n_z);
  R<<std_laspx_*std_laspx_,0,
          0,std_laspy_*std_laspy_;
  zp_cov = zp_cov+R;
  MatrixXd Tc = MatrixXd(n_x_,n_z);
  Tc.fill(0.0);
  for(int i=0;i<2*n_aug_+1;i++) {
    VectorXd x_diff = Xsig_pred_.col(i)-x_;
    VectorXd z_diff = zp_sig.col(i)-zp_mean;
    Tc=Tc+weights_(i)*x_diff*z_diff.transpose();
  }
  MatrixXd K = Tc*zp_cov.inverse();
  VectorXd zdiff = z_m-zp_mean;

  x_=x_+K*zdiff;
    while(x_(3)>M_PI){x_(3)-=2*M_PI;}
    while(x_(3)<-M_PI){x_(3)+=2*M_PI;}
  P_=P_-K*zp_cov*K.transpose();

  // calculate NIS:
  double nis = zdiff.transpose()*zp_cov.inverse()*zdiff;
  cout<<"NIS of laser:"<<nis<<endl;
    cout<<"x_(3) i.e. yaw_angle:"<<x_(3)<<endl;
}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */

  int n_z=3;
  VectorXd z_m = VectorXd(n_z);
  z_m<<meas_package.raw_measurements_(0),
          meas_package.raw_measurements_(1),
          meas_package.raw_measurements_(2);
  MatrixXd zp_sig = MatrixXd(n_z,2*n_aug_+1);
  VectorXd zp_mean = VectorXd(n_z);
  MatrixXd zp_cov = MatrixXd(n_z,n_z);
  zp_sig.fill(0.0);
  zp_cov.fill(0.0);

  // generate sigma points:
  for(int i=0;i<2*n_aug_+1;i++)
  {
    VectorXd xx = Xsig_pred_.col(i);
    zp_sig(0,i) = sqrt(xx(0)*xx(0)+xx(1)*xx(1));
    zp_sig(1,i) = atan2(xx(1),xx(0));
    zp_sig(2,i) = (xx(0)*cos(xx(3))*xx(2)+xx(1)*sin(xx(3))*xx(2))/zp_sig(0,i);
  }
  zp_mean=zp_sig*weights_;

  MatrixXd Tc = MatrixXd(n_x_,n_z);
  Tc.fill(0.0);
  for(int i=0;i<2*n_aug_+1;i++){
    VectorXd x_diff = Xsig_pred_.col(i)-x_;
    VectorXd z_diff = zp_sig.col(i)-zp_mean;

    while(z_diff(1)>M_PI){z_diff(1)-=2*M_PI;}
    while(z_diff(1)<-M_PI){z_diff(1)+=2*M_PI;}

    while(x_diff(3)>M_PI){x_diff(3)-=2*M_PI;}
    while(x_diff(3)<-M_PI){x_diff(3)+=2*M_PI;}

    zp_cov = zp_cov+weights_(i)*z_diff*z_diff.transpose();

    Tc=Tc+weights_(i)*x_diff*z_diff.transpose();
  }
  MatrixXd R = MatrixXd(n_z,n_z);
  R<<std_radr_*std_radr_,0,0,
          0,std_radphi_*std_radphi_,0,
          0,0,std_radrd_*std_radrd_;
  zp_cov = zp_cov+R;
  MatrixXd K = Tc*zp_cov.inverse();
  VectorXd zdiff = z_m-zp_mean;

  while(zdiff(1)>M_PI){zdiff(1)-=2*M_PI;}
  while(zdiff(1)<-M_PI){zdiff(1)+=2*M_PI;}

  x_=x_+K*zdiff;
    while(x_(3)>M_PI){x_(3)-=2*M_PI;}
    while(x_(3)<-M_PI){x_(3)+=2*M_PI;}
  P_=P_-K*zp_cov*K.transpose();

  // calcualte NIS:
  double nis = zdiff.transpose()*zp_cov.inverse()*zdiff;
  cout<<"NIS of Radar:"<<nis<<endl;
    cout<<"x_(3) i.e. yaw_angle:"<<x_(3)<<endl;
}
