#include "DJI_CameraTest.h"

void DJI_CameraTest::SetUp() {
  DJI_APITest::SetUp();

  activateDroneStandard();
  setControlStandard();

  camera = new Camera(DJI_APITest::api);

  // Wait for Gimbal to sync
  waitForGimbal();

  // Re-set gimbal angle to its init values
  gimbal = GimbalContainer(0,0,0,20,1);
  setGimbalAngleControl(gimbal);

  ASSERT_EQ(abs(camera->getGimbal().roll), 0);
  ASSERT_EQ(abs(camera->getGimbal().pitch), 0);
  ASSERT_EQ(abs(camera->getGimbal().yaw), 0);

  initialAngle.roll = camera->getGimbal().roll;
  initialAngle.pitch = camera->getGimbal().pitch;
  initialAngle.yaw = camera->getGimbal().yaw;
}

void DJI_CameraTest::TearDown() {
  releaseControlStandard();

  delete camera;
  DJI_APITest::TearDown();
}

::testing::AssertionResult ResultGimbalAngleNear(const int curAngle, const int precisionError, int delta) {
  if (precisionError <= delta)
    return ::testing::AssertionSuccess();
  else
    return ::testing::AssertionFailure() << "Current angle = " << curAngle
	<< ". Angle differ by more than " << delta << " degree(s)";
}

void DJI_CameraTest::doPrecisionErrorTest(ResultContainer *result){
  int length = sizeof(result->angle) / sizeof(int);
  for(int i = 0; i < length; i++)
    EXPECT_TRUE(ResultGimbalAngleNear(result->angle[i], result->error[i], PRECISION_DELTA));
}

/*
 * @brief
 *
 * Yaw angle unit 0.1 degrees, input range [-3200, 3200]
 * Roll angle unit 0.1 degrees, input range [-350, 350]
 * Pitch angle unit 0.1 degrees, input angle [-900, 300]
 *
 * isAbsolute bit - control flag:
 *
 * 0 - incremental control, the angle reference is the current Gimbal
 *  location
 *
 * 1 - absolute control, the angle reference is related to configuration
 * in DJI Go App
 *
 * yaw_cmd_ignore bit - Yaw invalid bit:
 *
 * 0 - gimbal will follow the command in Yaw
 *
 * 1 - gimbal will maintain position in Yaw
 *
 * roll_cmd_ignore bit - Roll invalid bit, same meaning as Yaw invalid bit
 * pitch_cmd_ignore bit - Pitch invalid bit, same meaning as Yaw invalid bit
 */
DJI_CameraTest::ResultContainer DJI_CameraTest::setGimbalAngleControl(GimbalContainer gimbal) {
  // Set control of the camera
  camera->setCamera(Camera::CAMERA_CODE::CODE_GIMBAL_ANGLE);

  GimbalAngleData gimbalAngle;
  gimbalAngle.roll = gimbal.roll;
  gimbalAngle.pitch = gimbal.pitch;
  gimbalAngle.yaw = gimbal.yaw;
  gimbalAngle.duration = gimbal.duration;
  gimbalAngle.mode |= 0;
  gimbalAngle.mode |= gimbal.isAbsolute;
  gimbalAngle.mode |= gimbal.yaw_cmd_ignore << 1;
  gimbalAngle.mode |= gimbal.roll_cmd_ignore << 2;
  gimbalAngle.mode |= gimbal.pitch_cmd_ignore << 3;

  // Get rotation angle for precision error assessment
  if(gimbal.isAbsolute)
    getInitialAngle(&gimbal);
  else
    getCurrentAngle(&gimbal);

  camera->setGimbalAngle(&gimbalAngle);
  // Give time for gimbal to sync
  sleep(4);

  return calculatePrecisionError(gimbal);
}

DJI_CameraTest::ResultContainer DJI_CameraTest::calculatePrecisionError(GimbalContainer gimbal) {
  ResultContainer result;
  int expectedAngle;

  // Calculate precision error
  for(int i = 0; i < 3; i++) {
    if(i == 0){
      result.angle[i] = camera->getGimbal().roll;
      if(gimbal.isAbsolute)
        expectedAngle = calculateAngle(gimbal.initialAngle.roll, gimbal.roll);
      else
        expectedAngle = calculateAngle(gimbal.currentAngle.roll, gimbal.roll);
    }else if(i == 1){
      result.angle[i] = camera->getGimbal().pitch;
      if(gimbal.isAbsolute)
        expectedAngle = calculateAngle(gimbal.initialAngle.pitch, gimbal.pitch);
      else
        expectedAngle = calculateAngle(gimbal.currentAngle.pitch, gimbal.pitch);
    }else if( i == 2){
      result.angle[i] = camera->getGimbal().yaw;
      if(gimbal.isAbsolute)
        expectedAngle = calculateAngle(gimbal.initialAngle.yaw, gimbal.yaw);
      else
        expectedAngle = calculateAngle(gimbal.currentAngle.yaw, gimbal.yaw);
    }
    result.error[i] = abs(expectedAngle - result.angle[i]);
  }
  return result;
}

/**
 * @brief
 *
 * Keep angle precision to integer values
 */
int DJI_CameraTest::calculateAngle(int initAngle, int newAngle) {
  int n = (newAngle / 10) + initAngle;
  int absAngle = std::abs(n);

  if(absAngle > 180 && n < 0)
    return n + 360;
  else if (absAngle > 180 && n > 0)
    return n - 360;
  else
    return n;
}

void DJI_CameraTest::getCurrentAngle(GimbalContainer *gimbal) {
  gimbal->currentAngle.roll = camera->getGimbal().roll;
  gimbal->currentAngle.pitch = camera->getGimbal().pitch;
  gimbal->currentAngle.yaw = camera->getGimbal().yaw;
}

void DJI_CameraTest::getInitialAngle(GimbalContainer *gimbal) {
  gimbal->initialAngle.roll = initialAngle.roll;
  gimbal->initialAngle.pitch = initialAngle.pitch;
  gimbal->initialAngle.yaw = initialAngle.yaw;
}

void DJI_CameraTest::waitForGimbal() {
  GimbalContainer::RotationAngle rAngle;

  do{
      rAngle.roll = camera->getGimbal().roll;
      rAngle.pitch = camera->getGimbal().pitch;
      rAngle.yaw = camera->getGimbal().yaw;
      sleep(1);
  }
  while(fabs(camera->getGimbal().roll - rAngle.roll) >= 0.099 ||
      fabs(camera->getGimbal().pitch - rAngle.pitch) >= 0.099 ||
      fabs(camera->getGimbal().yaw - rAngle.yaw) >= 0.099);
}

INSTANTIATE_TEST_CASE_P(
  setGimbalAngle, DJI_CameraTest, testing::Values(

    // Roll - absolute control
    /*
     * Roll angle fails to move to its min value of
     * -35 degrees. It goes only down to -29 degrees.
     */
    //GimbalContainer{-350, 0, 0, 20, 1},
    GimbalContainer{-250, 0, 0, 20, 1},
    GimbalContainer{-100, 0, 0, 20, 1},
    GimbalContainer{-55, 0, 0, 20, 1},
    /*
     * Roll angle fails to move to its max value of
     * 35 degrees. It goes only up to 29 degrees.
     */
    //GimbalContainer{350, 0, 0, 20, 1},
    GimbalContainer{250, 0, 0, 20, 1},
    GimbalContainer{100, 0, 0, 20, 1},
    GimbalContainer{55, 0, 0, 20, 1},

    // Roll - incremental control
    /*
     * Roll angle fails to move to its min value of
     * -35 degrees. It goes only down to -29 degrees.
     */
    //GimbalContainer{-350, 0, 0, 20, 0},
    GimbalContainer{-290, 0, 0, 20, 0},
    GimbalContainer{-280, 0, 0, 20, 0},
    GimbalContainer{-5, 0, 0, 20, 0},
    GimbalContainer{-45, 0, 0, 20, 0},

    // Pitch - absolute
    GimbalContainer{0, -300, 0, 20, 1},
    GimbalContainer{0, -500, 0, 20, 1},
    GimbalContainer{0, -700, 0, 20, 1},
    /**
     *@note
     *Rotating 90 degree in pitch direction will cause gimbal lock problem,
     *in which the value of roll and yaw are not reliable.
     */
    //GimbalContainer{0, -900, 0, 20, 1},
    GimbalContainer{0, 300, 0, 20, 1},
    GimbalContainer{0, 200, 0, 20, 1},
    GimbalContainer{0, 100, 0, 20, 1},

    // Pitch - incremental
    GimbalContainer{0, 300, 0, 20, 0},
    GimbalContainer{0, 200, 0, 20, 0},
    GimbalContainer{0, 100, 0, 20, 0},

    // Yaw - absolute
    GimbalContainer{0, 0, -1000, 20, 1},
    GimbalContainer{0, 0, 500, 20, 1},
    GimbalContainer{0, 0, -3100, 20, 1},
    GimbalContainer{0, 0, 2800, 20, 1},
    GimbalContainer{0, 0, -1200, 20, 1},

    // Yaw - incremental
    GimbalContainer{0, 0, -1000, 20, 0},
    GimbalContainer{0, 0, 500, 20, 0},
    GimbalContainer{0, 0, -3100, 20, 0},
    GimbalContainer{0, 0, 2800, 20, 0},
    GimbalContainer{0, 0, -1200, 20, 0},

    // Combinations
    GimbalContainer{45, 0, 2500, 20, 1},
    GimbalContainer{-45, 0, -2500, 20, 1},
    //GimbalContainer{290, 250, 1000, 20, 1},
    //GimbalContainer{-290, 250, -1000, 20, 1},
    //GimbalContainer{280, 250, 1000, 20, 1},
    //GimbalContainer{-280, -250, -1000, 20, 1},
    //GimbalContainer{0, 250, 1000, 20, 1},
    GimbalContainer{0, -250, 1000, 20, 1},
    GimbalContainer{45, 0, 2500, 20, 0},
    GimbalContainer{-45, 0, 2500, 20, 0},
    /*GimbalContainer{290, 250, 1000, 20, 0},
    GimbalContainer{-290, -250, -1000, 20, 0},
    GimbalContainer{280, 250, 1000, 20, 0},
    GimbalContainer{-280, -250, -1000, 20, 1},
    */
    GimbalContainer{0, 250, 1000, 20, 0},
    GimbalContainer{0, -250, 1000, 20, 0}
  ));

TEST_P(DJI_CameraTest, setGimbalAngle) {
  ResultContainer result = setGimbalAngleControl(GetParam());
  doPrecisionErrorTest(&result);
}

