/*!
 * @file PositionVelocityEstimator.h
 * @brief compute body position/velocity in world/body frames
 */ 



#ifndef PROJECT_POSITIONVELOCITYESTIMATOR_H
#define PROJECT_POSITIONVELOCITYESTIMATOR_H
#include "StateEstimatorContainer.h"

class CheaterPositionVelocityEstimator : public GenericEstimator{
  public:
    virtual void run();
    virtual void setup() {};
};


class LinearKFPositionVelocityEstimator : public GenericEstimator{
 public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  LinearKFPositionVelocityEstimator(){}
  virtual void run();
  virtual void setup();

 private:
  Eigen::Matrix<double, 18, 1> _xhat;
  Eigen::Matrix<double, 12, 1> _ps;
  Eigen::Matrix<double, 12, 1> _vs;
  Eigen::Matrix<double, 18, 18> _A;
  Eigen::Matrix<double, 18, 18> _Q0;
  Eigen::Matrix<double, 18, 18> _P;
  Eigen::Matrix<double, 28, 28> _R0;
  Eigen::Matrix<double, 18, 3> _B;
  Eigen::Matrix<double, 28, 18> _C;
};


#endif