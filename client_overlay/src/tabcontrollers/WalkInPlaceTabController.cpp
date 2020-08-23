#include "WalkInPlaceTabController.h"
#include <QQuickWindow>
#include <QApplication>
#include <QtQuick/QQuickView>
#include <QtQuick/QQuickItem>
#include <QtCore/QDebug>
#include <QtCore/QtMath>
#include "../overlaycontroller.h"
#include <openvr_math.h>
#include <chrono>

// application namespace
namespace walkinplace {

	WalkInPlaceTabController::~WalkInPlaceTabController() {
		if (identifyThread.joinable()) {
			identifyThread.join();
		}
	}


	void WalkInPlaceTabController::initStage1() {
		reloadProfiles();
		reloadSettings();
		gameType = std::make_shared <GameType>();
		gameType->inputType = InputType::touchpad;
	}


	void WalkInPlaceTabController::initStage2(OverlayController * parent, QQuickWindow * widget) {
		this->parent = parent;
		this->widget = widget;
	}


	void WalkInPlaceTabController::eventLoopTick() {
		if (!initializedDataModel) {
			if (!dataTrainingRequired) {
				loadDataModel();
			}
		}
		else if (wipEnabled) {
			if (!initializedDriver) {
				if (hasTwoControllers) {
					try {
						vrwalkinplace::VRWalkInPlace vrwalkinplace;
						vrwalkinplace.connect();
						vrwalkinplace.openvrEnableDriver(true);
						initializedDriver = true;
					}
					catch (std::exception& e) {
						LOG(INFO) << "Exception caught while adding initializing driver: " << e.what();
					}
				}
			}
			else {
				runSampleOnModel();
			}
		}
		if (!identifyDeviceQ.empty()) {
			auto now = std::chrono::duration_cast <std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			double tdiff = ((double)(now - timeLastHaptic));
			if (tdiff >= minHapticTO) {
				identifyDevice(identifyDeviceQ.front(), 2500);
			}
			tdiff = ((double)(now - identifyDeviceLast));
			if (tdiff >= identifyDeviceTO) {
				identifyDeviceQ.pop();
				identifyDeviceLast = now;
			}
		}
		if (!initializedDataModel || !initializedDriver || !wipEnabled) {
			for (uint32_t id = maxValidDeviceId; id < vr::k_unMaxTrackedDeviceCount; id++) {
				if (deviceIdSet.find(id) == deviceIdSet.end()) {
					auto deviceClass = vr::VRSystem()->GetTrackedDeviceClass(id);
					if (deviceClass != vr::TrackedDeviceClass_Invalid) {
						if (deviceClass == vr::TrackedDeviceClass_HMD || deviceClass == vr::TrackedDeviceClass_Controller || deviceClass == vr::TrackedDeviceClass_GenericTracker) {
							auto info = std::make_shared<DeviceInfo>();
							info->openvrId = id;
							info->deviceClass = deviceClass;
							char buffer[vr::k_unMaxPropertyStringSize];
							vr::ETrackedPropertyError pError = vr::TrackedProp_Success;
							vr::VRSystem()->GetStringTrackedDeviceProperty(id, vr::Prop_SerialNumber_String, buffer, vr::k_unMaxPropertyStringSize, &pError);
							if (pError == vr::TrackedProp_Success) {
								info->serial = std::string(buffer);
							}
							else {
								info->serial = std::string("<unknown serial>");
								LOG(ERROR) << "Could not get serial of device " << id;
							}
							deviceInfos.push_back(info);
							deviceIdSet.insert(id);
							if (deviceClass == vr::ETrackedDeviceClass::TrackedDeviceClass_HMD) {
								if (hmdID == vr::k_unTrackedDeviceIndexInvalid) {
									hmdID = info->openvrId;
								}
							}
							LOG(INFO) << "Found device: id " << info->openvrId << ", class " << info->deviceClass << ", serial " << info->serial;
						}
						maxValidDeviceId = id + 1;
					}
				}
				int cntrlIdx = 0;
				int trkrIdx = 0;
				int cntrlCount = 0;
				for (auto dev : deviceInfos) {
					if (dev->deviceClass == vr::ETrackedDeviceClass::TrackedDeviceClass_Controller) {
						cntrlCount++;
						if (cntrlCount >= 2) {
							hasTwoControllers = true;
						}
					}
					if (currentProfileIdx >= 0 && currentProfileIdx < walkInPlaceProfiles.size()) {
						if (dev->serial.find("ovrwip") == std::string::npos) {
							auto p = &walkInPlaceProfiles[currentProfileIdx];
							if (controller1ID == vr::k_unTrackedDeviceIndexInvalid && p->cntrl1Idx >= 0) {
								if (dev->deviceClass == p->cntrl1Type && cntrlIdx == p->cntrl1Idx) {
									controller1ID = dev->openvrId;
									dev->isTrackedAsCNTRL = true;
								}
							}
							else if (controller2ID == vr::k_unTrackedDeviceIndexInvalid && p->cntrl2Idx >= 0) {
								if (dev->deviceClass == p->cntrl2Type && cntrlIdx == p->cntrl2Idx) {
									controller2ID = dev->openvrId;
									dev->isTrackedAsCNTRL = true;
								}
							}
							if (tracker1ID == vr::k_unTrackedDeviceIndexInvalid && p->trkr1Idx >= 0) {
								if (dev->deviceClass == p->trkr1Type && trkrIdx == p->trkr1Idx) {
									tracker1ID = dev->openvrId;
									dev->isTrackedAsTRKR = true;
								}
							}
							else if (tracker2ID == vr::k_unTrackedDeviceIndexInvalid && p->trkr2Idx >= 0) {
								if (dev->deviceClass == p->trkr2Type && trkrIdx == p->trkr2Idx) {
									tracker2ID = dev->openvrId;
									dev->isTrackedAsTRKR = true;
								}
							}
							if (dev->deviceClass == p->cntrl1Type || dev->deviceClass == p->cntrl2Type) {
								cntrlIdx++;
							}
							else if (dev->deviceClass == p->trkr1Type || dev->deviceClass == p->trkr2Type) {
								trkrIdx++;
							}
						}
						else {
							ovrwipCNTRLID = dev->openvrId;
						}
					}
					else {
						if (dev->deviceClass == vr::ETrackedDeviceClass::TrackedDeviceClass_Controller) {
							if (dev->serial.find("ovrwip") == std::string::npos) {
								if (controller1ID == vr::k_unTrackedDeviceIndexInvalid) {
									controller1ID = dev->openvrId;
									dev->isTrackedAsCNTRL = true;
								}
								else if (controller2ID == vr::k_unTrackedDeviceIndexInvalid && dev->openvrId != controller1ID) {
									if (!initializedProfile) {
										controller2ID = dev->openvrId;
										dev->isTrackedAsCNTRL = true;
									}
								}
							}
							else {
								ovrwipCNTRLID = dev->openvrId;
							}
						}
						/*else if (dev->deviceClass == vr::ETrackedDeviceClass::TrackedDeviceClass_GenericTracker) {
							if (tracker1ID == vr::k_unTrackedDeviceIndexInvalid) {
								tracker1ID = dev->openvrId;
								//dev->isTrackedAsTRKR = true;
							}
							else if (tracker2ID == vr::k_unTrackedDeviceIndexInvalid && dev->openvrId != tracker1ID) {
								tracker2ID = dev->openvrId;
								//dev->isTrackedAsTRKR = true;
							}
						}*/
					}
				}
			}
		}
	}

	void WalkInPlaceTabController::handleEvent(const vr::VREvent_t&) {
		/*switch (vrEvent.eventType) {
		default:
		break;
		}*/
	}

	unsigned  WalkInPlaceTabController::getDeviceCount() {
		return (unsigned)deviceInfos.size();
	}

	QString WalkInPlaceTabController::getDeviceSerial(unsigned index) {
		if (index < deviceInfos.size()) {
			return QString::fromStdString(deviceInfos[index]->serial);
		}
		else {
			return QString("<ERROR>");
		}
	}

	unsigned WalkInPlaceTabController::getDeviceId(unsigned index) {
		if (index < deviceInfos.size()) {
			return (int)deviceInfos[index]->openvrId;
		}
		else {
			return vr::k_unTrackedDeviceIndexInvalid;
		}
	}

	int WalkInPlaceTabController::getDeviceClass(unsigned index) {
		if (index < deviceInfos.size()) {
			return (int)deviceInfos[index]->deviceClass;
		}
		else {
			return -1;
		}
	}

	bool WalkInPlaceTabController::isWIPEnabled() {
		return wipEnabled;
	}

	double WalkInPlaceTabController::getStepTime() {
		return 0;
	}

	int WalkInPlaceTabController::getGameType() {
		if (gameType->inputType == InputType::touchpad) {
			if (gameType->alwaysHeld) {
				return 2;
			}
			else if (gameType->useClick) {
				return 0;
			}
			else {
				return 1;
			}
		}
		else if (gameType->inputType == InputType::joystick) {
			if (gameType->alwaysHeld) {
				return 5;
			}
			else if (gameType->useClick) {
				return 3;
			}
			else {
				return 4;
			}
		}
		else if (gameType->inputType == InputType::grip) {
			return 6;
		}
		else if (gameType->inputType == InputType::keyWASD) {
			return 7;
		}
		else {
			return 8;
		}
	}

	int WalkInPlaceTabController::getHMDType() {
		return hmdType;
	}

	int WalkInPlaceTabController::getPaceControl() {
		return paceControl;
	}

	bool WalkInPlaceTabController::getDisableHMD() {
		return trackHMDVel;
	}

	bool WalkInPlaceTabController::getTrackHMDRot() {
		return trackHMDRot;
	}

	float WalkInPlaceTabController::getMinTouch() {
		return minTouch;
	}

	float WalkInPlaceTabController::getMidTouch() {
		return midTouch;
	}

	float WalkInPlaceTabController::getMaxTouch() {
		return maxTouch;
	}

	void WalkInPlaceTabController::clearSamplesAndModel() {
		dataModel.clear();
		hmdSample.clear();
		cntrlSample.clear();
		trkrSample.clear();
		dataModel.insert_cols(0, 1);
		dataModel.insert_rows(0, 10);
		hmdSample.insert_cols(0, 1);
		hmdSample.insert_rows(0, 1);
		cntrlSample.insert_cols(0, 1);
		cntrlSample.insert_rows(0, 2);
		trkrSample.insert_cols(0, 1);
		trkrSample.insert_rows(0, 2);
	}
	void WalkInPlaceTabController::loadDataModel() {
		try {
			bool loaded = dataModel.load(QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).absolutePath().toStdString() + "/" + model_file_name + model_file_type);
			if (loaded) {
				//dataModel = dataModel.t();
				initializedDataModel = true;
				dataTrainingRequired = false;
				hmdMaxPROTVel = arma::abs(dataModel.row(HMD_PITCH_VEL_IDX)).max();
				hmdMaxYROTVel = arma::abs(dataModel.row(HMD_YAW_VEL_IDX)).max();
				hmdMaxXVel = arma::abs(dataModel.row(HMD_X_VEL_IDX)).max();
				hmdMaxZVel = arma::abs(dataModel.row(HMD_Z_VEL_IDX)).max();
				modelCNTRL1 = arma::abs(dataModel.row(CNTRL1_Y_VEL_IDX));
				modelCNTRL2 = arma::abs(dataModel.row(CNTRL2_Y_VEL_IDX));
				minHMDPeakVal = findMinPeakMN(reqSNHMD, dataModel.row(HMD_Y_VEL_IDX), 0.007);
				maxCNTRLVal = std::max(modelCNTRL1.max(), modelCNTRL2.max());
				avgCNTRLVal = arma::sum(modelCNTRL1) / modelCNTRL1.n_cols;
				if (controller2ID != vr::k_unTrackedDeviceIndexInvalid) {
					avgCNTRLVal = (avgCNTRLVal + arma::sum(modelCNTRL2) / modelCNTRL2.n_cols) / 2.0;
				}
				maxTRKRPeakVal = dataModel.row(TRKR1_Y_VEL_IDX).max();
				float temp = dataModel.row(TRKR2_Y_VEL_IDX).max();
				maxTRKRPeakVal = temp > maxTRKRPeakVal ? temp : maxTRKRPeakVal;
				minTRKRPeakVal = findMinPeakMN(reqSNTRKR, dataModel.row(TRKR1_Y_VEL_IDX), 0.05);
				if (tracker2ID != vr::k_unTrackedDeviceIndexInvalid) {
					temp = findMinPeakMN(reqSNTRKR, dataModel.row(TRKR2_Y_VEL_IDX), 0.05);
					minTRKRPeakVal = temp < minTRKRPeakVal ? temp : minTRKRPeakVal;
				}
				/*
				mKAVGCNTRL.clear();
				mKAVGCNTRL.insert_rows(0, 2);
				//mKAVGCNTRL.insert_cols(0, 1);
				for (int k = 0; k < ((mCNTRL1.n_cols) / reqSNCNTRL); k++) {
					if ((k + 1) * reqSNCNTRL < mCNTRL1.n_cols - 1 && (k + 1) * reqSNCNTRL < mCNTRL2.n_cols - 1) {
						arma::rowvec mK1 = mCNTRL1.cols(k * reqSNCNTRL, (k + 1) * reqSNCNTRL);
						arma::rowvec mK2 = mCNTRL2.cols(k * reqSNCNTRL, (k + 1) * reqSNCNTRL);
						mKAVGCNTRL.insert_cols(0, 1);
						mKAVGCNTRL(0, k) = (arma::mean(mK1) + arma::mean(mK2)) / 2.0;
						mKAVGCNTRL(1, k) = std::floor((k * reqSNCNTRL) + (reqSNCNTRL / 2.0));
					}
				}*/
			}
			else {
				dataTrainingRequired = true;
				clearSamplesAndModel();
				LOG(INFO) << "unable to load data model at: " << QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).absolutePath().toStdString() + "/" + model_file_name + model_file_type;
			}
		}
		catch (std::exception& e) {
			initializedDataModel = false;
			dataTrainingRequired = true;
			LOG(INFO) << "Exception caught while loading data model: " << e.what();
		}
	}

	QList<qreal> WalkInPlaceTabController::getModelData() {
		bool loaded = initializedDataModel;
		if (!initializedDataModel) {
			loadDataModel();
		}
		QList<qreal> vals;
		if (loaded) {
			try {
				for (int i = 0; i < dataModel.n_cols; i++) {
					for (int j = 0; j < 5; j++) { // just the hmd, cntrl, tracker Y vel
						if (j < dataModel.n_rows) {
							vals.push_back((float)dataModel(j, i));
						}
					}
					vals.push_back((float)dataModel(TOUCH_VAL_IDX, i));
				}
			}
			catch (std::exception& e) {
				LOG(INFO) << "Exception caught while getting the data model: " << e.what();
			}
		}
		return vals;
	}

	void WalkInPlaceTabController::completeTraining() {
		try {
			dataModel = dataModel.t();
			mlpack::data::Save(QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).absolutePath().toStdString() + "/" + model_file_name + model_file_type, dataModel);
			clearSamplesAndModel();
			initializedDataModel = false;
			dataTrainingRequired = false;
		}
		catch (std::exception& e) {
			LOG(INFO) << "Exception caught while saving data model: " << e.what();
		}
	}

	QList<qreal> WalkInPlaceTabController::getHMDSample() {
		QList<qreal> vals;
		if (wipEnabled) {
			try {
				vals.push_back(validSample ? 1 : 0);
				vals.push_back(lastValidHMDSampleMKi);
				for (int i = 0; i < hmdSample.n_cols; i++) {
					vals.push_back((float)hmdSample(0, i));
					//vals.push_back((float)hmdSample(1, i));
					//vals.push_back((float)hmdSample(2, i));
				}
			}
			catch (std::exception& e) {
				LOG(INFO) << "Exception caught while getting the hmd sample: " << e.what();
			}
		}
		return vals;
	}

	QList<qreal> WalkInPlaceTabController::getCNTRLSample() {
		QList<qreal> vals;
		if (wipEnabled) {
			try {
				vals.push_back(validSample ? 1 : 0);
				vals.push_back(lastCNTRLSampleMKi);
				for (int i = 0; i < cntrlSample.n_cols; i++) {
					vals.push_back((float)cntrlSample(0, i));
					vals.push_back((float)cntrlSample(1, i));
				}
			}
			catch (std::exception& e) {
				LOG(INFO) << "Exception caught while getting the controller sample: " << e.what();
			}
		}

		return vals;
	}

	QList<qreal> WalkInPlaceTabController::getTRKRSample() {
		QList<qreal> vals;
		if (wipEnabled) {
			try {
				vals.push_back(validSample ? 1 : 0);
				vals.push_back(lastValidTRKRSampleMKi);
				for (int i = 0; i < trkrSample.n_cols; i++) {
					vals.push_back((float)trkrSample(0, i));
					vals.push_back((float)trkrSample(1, i));
				}
			}
			catch (std::exception& e) {
				LOG(INFO) << "Exception caught while getting the tracker sample: " << e.what();
			}
		}
		return vals;
	}

	QList<qreal> WalkInPlaceTabController::trainingDataSample(float scaleSpeed, double tdiff) {
		initializedDataModel = false;
		dataTrainingRequired = true;
		vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, latestDevicePoses, vr::k_unMaxTrackedDeviceCount);
		bool firstController = true;
		bool firstTracker = true;
		vr::HmdVector3d_t hmdVel = { 0, 0, 0 };
		vr::HmdVector3d_t hmdRotVel = { 0, 0, 0 };
		vr::HmdVector3d_t cont1Vel = { 0, 0, 0 };
		vr::HmdVector3d_t cont2Vel = { 0, 0, 0 };
		vr::HmdVector3d_t tracker1Vel = { 0, 0, 0 };
		vr::HmdVector3d_t tracker2Vel = { 0, 0, 0 };
		if (hmdType != 0) {
			auto m = latestDevicePoses[hmdID].mDeviceToAbsoluteTracking.m;

			hmdVel.v[0] = (m[0][3] - lastHmdPos.v[0]) / tdiff;
			hmdVel.v[1] = (m[1][3] - lastHmdPos.v[1]) / tdiff;
			hmdVel.v[2] = (m[2][3] - lastHmdPos.v[2]) / tdiff;

			lastHmdPos.v[0] = m[0][3];
			lastHmdPos.v[1] = m[1][3];
			lastHmdPos.v[2] = m[2][3];

		}
		else {
			hmdVel.v[0] = latestDevicePoses[hmdID].vVelocity.v[0];
			hmdVel.v[1] = latestDevicePoses[hmdID].vVelocity.v[1];
			hmdVel.v[2] = latestDevicePoses[hmdID].vVelocity.v[2];
		}

		vr::HmdQuaternion_t qRotation = vrmath::quaternionFromRotationMatrix(latestDevicePoses[hmdID].mDeviceToAbsoluteTracking);
		vr::HmdVector3d_t forward = { 0,0,-1 };
		vr::HmdVector3d_t right = { 1,0,0 };
		vr::HmdVector3d_t devForward = vrmath::quaternionRotateVector(qRotation, forward);
		vr::HmdVector3d_t devRight = vrmath::quaternionRotateVector(qRotation, right);

		//hmdVel = vrmath::quaternionRotateVector(qRotation, hmdVel);

		double devYaw = (180 * std::asin(devForward.v[0])) / M_PI;
		double devPitch = (180 * std::asin(devForward.v[1])) / M_PI;
		double devRoll = (180 * std::asin(devRight.v[1])) / M_PI;

		if (dataModel.n_cols > 5) {
			hmdRotVel.v[0] = (devYaw - lastHmdRot.v[0]) / tdiff;
			hmdRotVel.v[1] = (devPitch - lastHmdRot.v[1]) / tdiff;
			hmdRotVel.v[2] = (devRoll - lastHmdRot.v[2]) / tdiff;
		}

		lastHmdRot.v[0] = devYaw;
		lastHmdRot.v[1] = devPitch;
		lastHmdRot.v[2] = devRoll;

		if (controller1ID != vr::k_unTrackedDeviceIndexInvalid) {
			cont1Vel.v[0] = latestDevicePoses[controller1ID].vVelocity.v[0];
			cont1Vel.v[1] = latestDevicePoses[controller1ID].vVelocity.v[1];
			cont1Vel.v[2] = latestDevicePoses[controller1ID].vVelocity.v[2];
			qRotation = vrmath::quaternionFromRotationMatrix(latestDevicePoses[controller1ID].mDeviceToAbsoluteTracking);
			devForward = vrmath::quaternionRotateVector(qRotation, forward);
			devRight = vrmath::quaternionRotateVector(qRotation, right);

			//cont1Vel = vrmath::quaternionRotateVector(qRotation, cont1Vel);

			devYaw = (180 * std::asin(devForward.v[0])) / M_PI;
			devPitch = (180 * std::asin(devForward.v[1])) / M_PI;
			devRoll = (180 * std::asin(devRight.v[1])) / M_PI;

			if (controller2ID != vr::k_unTrackedDeviceIndexInvalid) {
				cont2Vel.v[0] = latestDevicePoses[controller2ID].vVelocity.v[0];
				cont2Vel.v[1] = latestDevicePoses[controller2ID].vVelocity.v[1];
				cont2Vel.v[2] = latestDevicePoses[controller2ID].vVelocity.v[2];
				qRotation = vrmath::quaternionFromRotationMatrix(latestDevicePoses[controller2ID].mDeviceToAbsoluteTracking);
				devForward = vrmath::quaternionRotateVector(qRotation, forward);
				devRight = vrmath::quaternionRotateVector(qRotation, right);

				//cont2Vel = vrmath::quaternionRotateVector(qRotation, cont2Vel);

				devYaw = (180 * std::asin(devForward.v[0])) / M_PI;
				devPitch = (180 * std::asin(devForward.v[1])) / M_PI;
				devRoll = (180 * std::asin(devRight.v[1])) / M_PI;
			}
		}

		if (tracker1ID != vr::k_unTrackedDeviceIndexInvalid) {
			tracker1Vel.v[0] = latestDevicePoses[tracker1ID].vVelocity.v[0];
			tracker1Vel.v[1] = latestDevicePoses[tracker1ID].vVelocity.v[1];
			tracker1Vel.v[2] = latestDevicePoses[tracker1ID].vVelocity.v[2];
			qRotation = vrmath::quaternionFromRotationMatrix(latestDevicePoses[tracker1ID].mDeviceToAbsoluteTracking);
			devForward = vrmath::quaternionRotateVector(qRotation, forward);
			devRight = vrmath::quaternionRotateVector(qRotation, right);

			//tracker1Vel = vrmath::quaternionRotateVector(qRotation, tracker1Vel);

			devYaw = (180 * std::asin(devForward.v[0])) / M_PI;
			devPitch = (180 * std::asin(devForward.v[1])) / M_PI;
			devRoll = (180 * std::asin(devRight.v[1])) / M_PI;

			if (tracker2ID != vr::k_unTrackedDeviceIndexInvalid) {
				tracker2Vel.v[0] = latestDevicePoses[tracker2ID].vVelocity.v[0];
				tracker2Vel.v[1] = latestDevicePoses[tracker2ID].vVelocity.v[1];
				tracker2Vel.v[2] = latestDevicePoses[tracker2ID].vVelocity.v[2];
				qRotation = vrmath::quaternionFromRotationMatrix(latestDevicePoses[tracker2ID].mDeviceToAbsoluteTracking);
				devForward = vrmath::quaternionRotateVector(qRotation, forward);
				devRight = vrmath::quaternionRotateVector(qRotation, right);

				//tracker2Vel = vrmath::quaternionRotateVector(qRotation, tracker2Vel);

				devYaw = (180 * std::asin(devForward.v[0])) / M_PI;
				devPitch = (180 * std::asin(devForward.v[1])) / M_PI;
				devRoll = (180 * std::asin(devRight.v[1])) / M_PI;
			}
		}
		try {
			int n = dataModel.n_cols;
			dataModel.insert_cols(n, 1);

			dataModel(HMD_X_VEL_IDX, n) = hmdVel.v[0];
			dataModel(HMD_Y_VEL_IDX, n) = hmdVel.v[1];
			dataModel(HMD_Z_VEL_IDX, n) = hmdVel.v[2];
			dataModel(CNTRL1_Y_VEL_IDX, n) = cont1Vel.v[1];
			dataModel(CNTRL2_Y_VEL_IDX, n) = cont2Vel.v[1];
			dataModel(TRKR1_Y_VEL_IDX, n) = tracker1Vel.v[1];
			dataModel(TRKR2_Y_VEL_IDX, n) = tracker2Vel.v[1];
			dataModel(HMD_YAW_VEL_IDX, n) = hmdRotVel.v[0]; // yaw rotational velocity
			dataModel(HMD_PITCH_VEL_IDX, n) = hmdRotVel.v[1]; // pitch rotational velocity
			dataModel(TOUCH_VAL_IDX, n) = scaleSpeed;
		}
		catch (std::exception& e) {
			LOG(INFO) << "Exception caught while building data model: " << e.what();
		}

		QList<qreal> vals;
		vals.push_back(hmdRotVel.v[0]);// hmdVel.v[0]); // yaw vel
		vals.push_back(hmdVel.v[1]);
		vals.push_back(hmdRotVel.v[1]);//hmdVel.v[2]); // pitch vel

		vals.push_back(cont1Vel.v[1]);
		vals.push_back(cont2Vel.v[1]);

		vals.push_back(tracker1Vel.v[1]);
		vals.push_back(tracker2Vel.v[1]);
		//LOG(INFO) << "HMD VALS: " << hmdVel.v[0] << "," << hmdVel.v[1] << "," << hmdVel.v[2];	

		return vals;
	}

	QList<qreal> WalkInPlaceTabController::getGraphPoses(double tdiff) {
		vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, latestDevicePoses, vr::k_unMaxTrackedDeviceCount);
		auto now = std::chrono::duration_cast <std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		bool firstController = true;
		bool firstTracker = true;
		vr::HmdVector3d_t hmdVel = { 0, 0, 0 };
		vr::HmdVector3d_t hmdRotVel = { 0, 0, 0 };
		vr::HmdVector3d_t cont1Vel = { 0, 0, 0 };
		vr::HmdVector3d_t cont2Vel = { 0, 0, 0 };
		vr::HmdVector3d_t tracker1Vel = { 0, 0, 0 };
		vr::HmdVector3d_t tracker2Vel = { 0, 0, 0 };
		if (hmdType != 0) {
			auto m = latestDevicePoses[hmdID].mDeviceToAbsoluteTracking.m;

			hmdVel.v[0] = (m[0][3] - lastHmdPos.v[0]) / tdiff;
			hmdVel.v[1] = (m[1][3] - lastHmdPos.v[1]) / tdiff;
			hmdVel.v[2] = (m[2][3] - lastHmdPos.v[2]) / tdiff;

			lastHmdPos.v[0] = m[0][3];
			lastHmdPos.v[1] = m[1][3];
			lastHmdPos.v[2] = m[2][3];

		}
		else {
			hmdVel.v[0] = latestDevicePoses[hmdID].vVelocity.v[0];
			hmdVel.v[1] = latestDevicePoses[hmdID].vVelocity.v[1];
			hmdVel.v[2] = latestDevicePoses[hmdID].vVelocity.v[2];
		}

		vr::HmdQuaternion_t qRotation = vrmath::quaternionFromRotationMatrix(latestDevicePoses[hmdID].mDeviceToAbsoluteTracking);
		vr::HmdVector3d_t forward = { 0,0,-1 };
		vr::HmdVector3d_t right = { 1,0,0 };
		vr::HmdVector3d_t devForward = vrmath::quaternionRotateVector(qRotation, forward);
		vr::HmdVector3d_t devRight = vrmath::quaternionRotateVector(qRotation, right);

		//hmdVel = vrmath::quaternionRotateVector(qRotation, hmdVel);

		double devYaw = (180 * std::asin(devForward.v[0])) / M_PI;
		double devPitch = (180 * std::asin(devForward.v[1])) / M_PI;
		double devRoll = (180 * std::asin(devRight.v[1])) / M_PI;

		hmdRotVel.v[0] = (devYaw - lastHmdRot.v[0]) / tdiff;
		hmdRotVel.v[1] = (devPitch - lastHmdRot.v[1]) / tdiff;
		hmdRotVel.v[2] = (devRoll - lastHmdRot.v[2]) / tdiff;

		lastHmdRot.v[0] = devYaw;
		lastHmdRot.v[1] = devPitch;
		lastHmdRot.v[2] = devRoll;

		cont1Vel.v[0] = latestDevicePoses[controller1ID].vVelocity.v[0];
		cont1Vel.v[1] = latestDevicePoses[controller1ID].vVelocity.v[1];
		cont1Vel.v[2] = latestDevicePoses[controller1ID].vVelocity.v[2];
		qRotation = vrmath::quaternionFromRotationMatrix(latestDevicePoses[controller1ID].mDeviceToAbsoluteTracking);
		devForward = vrmath::quaternionRotateVector(qRotation, forward);
		devRight = vrmath::quaternionRotateVector(qRotation, right);

		//cont1Vel = vrmath::quaternionRotateVector(qRotation, cont1Vel);

		devYaw = (180 * std::asin(devForward.v[0])) / M_PI;
		devPitch = (180 * std::asin(devForward.v[1])) / M_PI;
		devRoll = (180 * std::asin(devRight.v[1])) / M_PI;

		if (controller2ID != vr::k_unTrackedDeviceIndexInvalid) {
			cont2Vel.v[0] = latestDevicePoses[controller2ID].vVelocity.v[0];
			cont2Vel.v[1] = latestDevicePoses[controller2ID].vVelocity.v[1];
			cont2Vel.v[2] = latestDevicePoses[controller2ID].vVelocity.v[2];
			qRotation = vrmath::quaternionFromRotationMatrix(latestDevicePoses[controller2ID].mDeviceToAbsoluteTracking);
			devForward = vrmath::quaternionRotateVector(qRotation, forward);
			devRight = vrmath::quaternionRotateVector(qRotation, right);

			//cont2Vel = vrmath::quaternionRotateVector(qRotation, cont2Vel);

			devYaw = (180 * std::asin(devForward.v[0])) / M_PI;
			devPitch = (180 * std::asin(devForward.v[1])) / M_PI;
			devRoll = (180 * std::asin(devRight.v[1])) / M_PI;
		}

		if (tracker1ID != vr::k_unTrackedDeviceIndexInvalid) {
			tracker1Vel.v[0] = latestDevicePoses[tracker1ID].vVelocity.v[0];
			tracker1Vel.v[1] = latestDevicePoses[tracker1ID].vVelocity.v[1];
			tracker1Vel.v[2] = latestDevicePoses[tracker1ID].vVelocity.v[2];
			qRotation = vrmath::quaternionFromRotationMatrix(latestDevicePoses[tracker1ID].mDeviceToAbsoluteTracking);
			devForward = vrmath::quaternionRotateVector(qRotation, forward);
			devRight = vrmath::quaternionRotateVector(qRotation, right);

			//tracker1Vel = vrmath::quaternionRotateVector(qRotation, tracker1Vel);

			devYaw = (180 * std::asin(devForward.v[0])) / M_PI;
			devPitch = (180 * std::asin(devForward.v[1])) / M_PI;
			devRoll = (180 * std::asin(devRight.v[1])) / M_PI;

			if (tracker2ID != vr::k_unTrackedDeviceIndexInvalid) {
				tracker2Vel.v[0] = latestDevicePoses[tracker2ID].vVelocity.v[0];
				tracker2Vel.v[1] = latestDevicePoses[tracker2ID].vVelocity.v[1];
				tracker2Vel.v[2] = latestDevicePoses[tracker2ID].vVelocity.v[2];
				qRotation = vrmath::quaternionFromRotationMatrix(latestDevicePoses[tracker2ID].mDeviceToAbsoluteTracking);
				devForward = vrmath::quaternionRotateVector(qRotation, forward);
				devRight = vrmath::quaternionRotateVector(qRotation, right);

				//tracker2Vel = vrmath::quaternionRotateVector(qRotation, tracker2Vel);

				devYaw = (180 * std::asin(devForward.v[0])) / M_PI;
				devPitch = (180 * std::asin(devForward.v[1])) / M_PI;
				devRoll = (180 * std::asin(devRight.v[1])) / M_PI;
			}
		}

		QList<qreal> vals;
		vals.push_back(hmdRotVel.v[0]);//hmdVel.v[0]); // yaw vel
		vals.push_back(hmdVel.v[1]);
		vals.push_back(hmdRotVel.v[1]);//hmdVel.v[2]); // pitch vel

		vals.push_back(cont1Vel.v[1]);
		vals.push_back(cont2Vel.v[1]);

		vals.push_back(tracker1Vel.v[1]);
		vals.push_back(tracker2Vel.v[1]);

		return vals;

	}

	bool WalkInPlaceTabController::isStepDetected() {
		return validSample;
	}

	QList<QString> WalkInPlaceTabController::getDataModelNames() {
		QList<QString> models;
		std::string model_loc = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).absolutePath().toStdString();
		try {
			fs::directory_iterator end_iter;
			for (fs::directory_iterator dir_itr(model_loc); dir_itr != end_iter; ++dir_itr) {
				try {
					if (fs::is_regular_file(dir_itr->status())) {
						if (dir_itr->path().extension().generic_string().find("csv") != std::string::npos) {
							models.push_back(QString::fromStdString(dir_itr->path().filename().generic_string()));
						}
					}
				}
				catch (const std::exception & ex) {
				}
			}
		}
		catch (const std::exception & ex) {
			LOG(INFO) << "Error while getting data model names: " << ex.what();
		}
		return models;
	}

	void WalkInPlaceTabController::addDataModel(QString name) {
		model_file_name = name.toStdString();
		clearSamplesAndModel();
	}

	void WalkInPlaceTabController::applyDataModel(QString name) {
		model_file_name = name.toStdString();
		clearSamplesAndModel();
		initializedDataModel = false;
		dataTrainingRequired = false;
	}

	void WalkInPlaceTabController::deleteDataModel(QString name) {
		std::string model_loc = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).absolutePath().toStdString() + "/" + name.toStdString() + model_file_type;
		if (std::remove(model_loc.c_str()) != 0) {
			LOG(INFO) << "could not delete, you can delete manually from: " << model_loc;
		}
		clearSamplesAndModel();
	}

	void WalkInPlaceTabController::reloadSettings() {
		auto settings = OverlayController::appSettings();
		settings->beginGroup("walkInPlaceSettings");
		settings->endGroup();
	}

	void WalkInPlaceTabController::reloadProfiles() {
		walkInPlaceProfiles.clear();
		auto settings = OverlayController::appSettings();
		settings->beginGroup("walkInPlaceSettings");
		auto profileCount = settings->beginReadArray("walkInPlaceProfiles");
		for (int i = 0; i < profileCount; i++) {
			settings->setArrayIndex(i);
			walkInPlaceProfiles.emplace_back();
			auto& entry = walkInPlaceProfiles[i];
			entry.profileName = settings->value("profileName").toString().toStdString();
			entry.modelFile = settings->value("modelFile", QString((model_file_name).c_str())).toString().toStdString();
			entry.wipEnabled = settings->value("wipEnabled", false).toBool();
			entry.gameType = settings->value("gameType", 0).toInt();
			entry.hmdType = settings->value("hmdType", 0).toInt();
			entry.trackHMDVel = settings->value("trackHMDVel", true).toBool();
			entry.trackHMDRot = settings->value("trackHMDRot", true).toBool();
			entry.minTouch = settings->value("minTouch", 0.25).toFloat();
			entry.midTouch = settings->value("midTouch", 0.83).toFloat();
			entry.maxTouch = settings->value("maxTouch", 1).toFloat();
			entry.cntrl1Idx = settings->value("cntrl1Idx", 0).toInt();
			entry.cntrl2Idx = settings->value("cntrl2Idx", -1).toInt();
			entry.trkr1Idx = settings->value("trkr1Idx", -1).toInt();
			entry.trkr2Idx = settings->value("trkr2Idx", -1).toInt();
			entry.cntrl1Type = static_cast<vr::ETrackedDeviceClass>(settings->value("cntrl1Type", vr::TrackedDeviceClass::TrackedDeviceClass_Controller).toInt());
			entry.cntrl2Type = static_cast<vr::ETrackedDeviceClass>(settings->value("cntrl2Type", vr::TrackedDeviceClass::TrackedDeviceClass_Controller).toInt());
			entry.trkr1Type = static_cast<vr::ETrackedDeviceClass>(settings->value("trkr1Type", vr::TrackedDeviceClass::TrackedDeviceClass_GenericTracker).toInt());
			entry.trkr2Type = static_cast<vr::ETrackedDeviceClass>(settings->value("trkr2Type", vr::TrackedDeviceClass::TrackedDeviceClass_GenericTracker).toInt());
		}
		settings->endArray();
		settings->endGroup();
	}

	void WalkInPlaceTabController::saveSettings() {
		auto settings = OverlayController::appSettings();
		settings->beginGroup("walkInPlaceSettings");
		settings->endGroup();
		settings->sync();
	}


	void WalkInPlaceTabController::saveProfiles() {
		auto settings = OverlayController::appSettings();
		settings->beginGroup("walkInPlaceSettings");
		settings->beginWriteArray("walkInPlaceProfiles");
		unsigned i = 0;
		for (auto& p : walkInPlaceProfiles) {
			settings->setArrayIndex(i);
			settings->setValue("profileName", QString::fromStdString(p.profileName));
			settings->setValue("modelFile", QString::fromStdString(p.modelFile));
			settings->setValue("wipEnabled", p.wipEnabled);
			settings->setValue("gameType", p.gameType);
			settings->setValue("hmdType", p.hmdType);
			settings->setValue("trackHMDVel", p.trackHMDVel);
			settings->setValue("trackHMDRot", p.trackHMDRot);
			settings->setValue("minTouch", p.minTouch);
			settings->setValue("midTouch", p.midTouch);
			settings->setValue("maxTouch", p.maxTouch);
			settings->setValue("cntrl1Idx", p.cntrl1Idx);
			settings->setValue("cntrl2Idx", p.cntrl2Idx);
			settings->setValue("trkr1Idx", p.trkr1Idx);
			settings->setValue("trkr2Idx", p.trkr2Idx);
			settings->setValue("cntrl1Type", p.cntrl1Type);
			settings->setValue("cntrl2Type", p.cntrl2Type);
			settings->setValue("trkr1Type", p.trkr1Type);
			settings->setValue("trkr2Type", p.trkr2Type);
			i++;
		}
		settings->endArray();
		settings->endGroup();
		settings->sync();
	}

	unsigned WalkInPlaceTabController::getProfileCount() {
		return (unsigned)walkInPlaceProfiles.size();
	}

	QString WalkInPlaceTabController::getProfileName(unsigned index) {
		if (index >= walkInPlaceProfiles.size()) {
			return QString();
		}
		else {
			return QString::fromStdString(walkInPlaceProfiles[index].profileName);
		}
	}

	void WalkInPlaceTabController::addProfile(QString name) {
		WalkInPlaceProfile* profile = nullptr;
		for (auto& p : walkInPlaceProfiles) {
			if (p.profileName.compare(name.toStdString()) == 0) {
				profile = &p;
				break;
			}
		}
		if (!profile) {
			auto i = walkInPlaceProfiles.size();
			walkInPlaceProfiles.emplace_back();
			profile = &walkInPlaceProfiles[i];
		}
		profile->profileName = name.toStdString();
		profile->wipEnabled = isWIPEnabled();
		profile->modelFile = (model_file_name);
		profile->gameType = getGameType();
		profile->hmdType = hmdType;
		profile->trackHMDVel = trackHMDVel;
		profile->trackHMDRot = trackHMDRot;
		profile->minTouch = minTouch;
		profile->midTouch = midTouch;
		profile->maxTouch = maxTouch;
		int cntrlIdx = 0;
		int trkrIdx = -1;
		bool cntrl1Saved = false;
		bool trkr1Saved = false;
		for (int i = 0; i < 4; i++) {
			if (getDeviceEnabled(2, i, 0)) {
				if (!cntrl1Saved) {
					profile->cntrl1Idx = i;
					profile->cntrl1Type = vr::ETrackedDeviceClass::TrackedDeviceClass_Controller;
					cntrl1Saved = true;
				}
				else {
					profile->cntrl2Idx = i;
					profile->cntrl2Type = vr::ETrackedDeviceClass::TrackedDeviceClass_Controller;
				}
			}
			if (getDeviceEnabled(2, i, 1)) {
				if (!trkr1Saved) {
					profile->trkr1Idx = i;
					profile->trkr1Type = vr::ETrackedDeviceClass::TrackedDeviceClass_Controller;
					trkr1Saved = true;
				}
				else {
					profile->trkr2Idx = i;
					profile->trkr2Type = vr::ETrackedDeviceClass::TrackedDeviceClass_Controller;
				}
			}
			if (getDeviceEnabled(3, i, 0)) {
				if (!cntrl1Saved) {
					profile->cntrl1Idx = i;
					profile->cntrl1Type = vr::ETrackedDeviceClass::TrackedDeviceClass_GenericTracker;
					cntrl1Saved = true;
				}
				else {
					profile->cntrl2Idx = i;
					profile->cntrl2Type = vr::ETrackedDeviceClass::TrackedDeviceClass_GenericTracker;
				}
			}
			if (getDeviceEnabled(3, i, 1)) {
				if (!trkr1Saved) {
					profile->trkr1Idx = i;
					profile->trkr1Type = vr::ETrackedDeviceClass::TrackedDeviceClass_GenericTracker;
					trkr1Saved = true;
				}
				else {
					profile->trkr2Idx = i;
					profile->trkr2Type = vr::ETrackedDeviceClass::TrackedDeviceClass_GenericTracker;
				}
			}
		}
		saveProfiles();
		OverlayController::appSettings()->sync();
	}

	void WalkInPlaceTabController::applyProfile(unsigned index) {
		if (index < walkInPlaceProfiles.size()) {
			currentProfileIdx = index;
			auto& profile = walkInPlaceProfiles[index];
			hmdType = profile.hmdType;
			trackHMDVel = profile.trackHMDVel;
			minTouch = profile.minTouch;
			midTouch = profile.midTouch;
			maxTouch = profile.maxTouch;

			enableWIP(profile.wipEnabled);
			setGameType(profile.gameType);
			setHMDType(profile.hmdType);
			setTrackHMDVel(profile.trackHMDVel);
			setTrackHMDRot(profile.trackHMDRot);
			setMinTouch(profile.minTouch);
			setMidTouch(profile.midTouch);
			setMaxTouch(profile.maxTouch);

			enableDevice(vr::ETrackedDeviceClass::TrackedDeviceClass_Controller, 0, false, TrackedDeviceLoc::TrackedDeviceOnHands);
			enableDevice(vr::ETrackedDeviceClass::TrackedDeviceClass_Controller, 1, false, TrackedDeviceLoc::TrackedDeviceOnHands);
			enableDevice(vr::ETrackedDeviceClass::TrackedDeviceClass_Controller, 2, false, TrackedDeviceLoc::TrackedDeviceOnHands);
			enableDevice(vr::ETrackedDeviceClass::TrackedDeviceClass_Controller, 3, false, TrackedDeviceLoc::TrackedDeviceOnHands);
			enableDevice(vr::ETrackedDeviceClass::TrackedDeviceClass_GenericTracker, 0, false, TrackedDeviceLoc::TrackedDeviceOnHands);
			enableDevice(vr::ETrackedDeviceClass::TrackedDeviceClass_GenericTracker, 1, false, TrackedDeviceLoc::TrackedDeviceOnHands);
			enableDevice(vr::ETrackedDeviceClass::TrackedDeviceClass_GenericTracker, 2, false, TrackedDeviceLoc::TrackedDeviceOnHands);
			enableDevice(vr::ETrackedDeviceClass::TrackedDeviceClass_GenericTracker, 3, false, TrackedDeviceLoc::TrackedDeviceOnHands);
			enableDevice(vr::ETrackedDeviceClass::TrackedDeviceClass_Controller, 0, false, TrackedDeviceLoc::TrackedDeviceOnFeet);
			enableDevice(vr::ETrackedDeviceClass::TrackedDeviceClass_Controller, 1, false, TrackedDeviceLoc::TrackedDeviceOnFeet);
			enableDevice(vr::ETrackedDeviceClass::TrackedDeviceClass_Controller, 2, false, TrackedDeviceLoc::TrackedDeviceOnFeet);
			enableDevice(vr::ETrackedDeviceClass::TrackedDeviceClass_Controller, 3, false, TrackedDeviceLoc::TrackedDeviceOnFeet);
			enableDevice(vr::ETrackedDeviceClass::TrackedDeviceClass_GenericTracker, 0, false, TrackedDeviceLoc::TrackedDeviceOnFeet);
			enableDevice(vr::ETrackedDeviceClass::TrackedDeviceClass_GenericTracker, 1, false, TrackedDeviceLoc::TrackedDeviceOnFeet);
			enableDevice(vr::ETrackedDeviceClass::TrackedDeviceClass_GenericTracker, 2, false, TrackedDeviceLoc::TrackedDeviceOnFeet);
			enableDevice(vr::ETrackedDeviceClass::TrackedDeviceClass_GenericTracker, 3, false, TrackedDeviceLoc::TrackedDeviceOnFeet);
			if (profile.cntrl1Idx >= 0) {
				controller1ID = vr::k_unTrackedDeviceIndexInvalid;
				enableDevice(profile.cntrl1Type, profile.cntrl1Idx, true, TrackedDeviceLoc::TrackedDeviceOnHands);
				if (profile.cntrl2Idx >= 0) {
					controller2ID = vr::k_unTrackedDeviceIndexInvalid;
					enableDevice(profile.cntrl2Type, profile.cntrl2Idx, true, TrackedDeviceLoc::TrackedDeviceOnHands);
				}
				else {
					controller2ID = vr::k_unTrackedDeviceIndexInvalid;
				}
			}
			else {
				controller1ID = vr::k_unTrackedDeviceIndexInvalid;
				//enableDevice(profile.cntrl1Type, profile.cntrl1Idx, false, 0);
			}
			if (profile.trkr1Idx >= 0) {
				tracker1ID = vr::k_unTrackedDeviceIndexInvalid;
				enableDevice(profile.trkr1Type, profile.trkr1Idx, true, TrackedDeviceLoc::TrackedDeviceOnFeet);
				if (profile.trkr2Idx >= 0) {
					tracker2ID = vr::k_unTrackedDeviceIndexInvalid;
					enableDevice(profile.trkr2Type, profile.trkr2Idx, true, TrackedDeviceLoc::TrackedDeviceOnFeet);
				}
				else {
					tracker2ID = vr::k_unTrackedDeviceIndexInvalid;
				}
			}
			else {
				tracker1ID = vr::k_unTrackedDeviceIndexInvalid;
				tracker2ID = vr::k_unTrackedDeviceIndexInvalid;
			}

			initializedProfile = true;

			auto settings = OverlayController::appSettings();
			settings->beginGroup("walkInPlaceSettings");
			auto profileCount = settings->beginReadArray("walkInPlaceProfiles");
			for (int i = 0; i < profileCount; i++) {
				settings->setArrayIndex(i);
				if (index == i) {
					maxSNHMD = settings->value("maxHMDSampleSize", 16).toInt();
					startSNHMD = settings->value("startHMDSampleSize", 12).toInt();
					reqSNHMD = settings->value("ongoingHMDSampleSize", 4).toInt();
					maxSNTRKR = settings->value("maxTRKRSampleSize", 23).toInt();
					startSNTRKR = settings->value("startTRKRSampleSize", 18).toInt();
					reqSNTRKR = settings->value("ongoingTRKRSampleSize", 12).toInt();
					maxSNCNTRL = settings->value("maxCNTRLSampleSize", 12).toInt();
					reqSNCNTRL = settings->value("ongoingCNTRLSampleSize", 10).toInt();
					hmdVelVariance = settings->value("hmdVelVariance", 0.07).toFloat();
					hmdMinDVPerSN = settings->value("hmdAccelPercentAccurate", 0.75).toFloat();
					trkrVariance = settings->value("trkrVelVariance", 0.37).toFloat();
					cntrlVariance = settings->value("cntrlVelVariance", 0.37).toFloat();
				}
			}
			settings->endArray();
			settings->endGroup();

			applyDataModel(QString::fromStdString(profile.modelFile));
		}
	}

	void WalkInPlaceTabController::deleteProfile(unsigned index) {
		if (index < walkInPlaceProfiles.size()) {
			initializedProfile = true;
			auto pos = walkInPlaceProfiles.begin() + index;
			walkInPlaceProfiles.erase(pos);
			saveProfiles();
			OverlayController::appSettings()->sync();
		}
	}

	void WalkInPlaceTabController::enableWIP(bool enable) {
		wipEnabled = enable;
		if (!enable && initializedDriver) {
			stopMovement();
		}
	}

	void WalkInPlaceTabController::setMinTouch(float value) {
		minTouch = value;
	}

	void WalkInPlaceTabController::setMidTouch(float value) {
		midTouch = value;
	}

	void WalkInPlaceTabController::setMaxTouch(float value) {
		maxTouch = value;
	}

	void WalkInPlaceTabController::setTrackHMDVel(bool value) {
		trackHMDVel = value;
	}

	void WalkInPlaceTabController::setTrackHMDRot(bool value) {
		trackHMDRot = value;
	}

	void WalkInPlaceTabController::setHMDMaxVari(float value) {
		hmdVelVariance = value;
	}

	bool WalkInPlaceTabController::getDeviceEnabled(int devClass, int devIdx, int mode) {
		int classIdx = 0;
		for (auto d : deviceInfos) {
			if (d->deviceClass == devClass) {
				if (classIdx == devIdx) {
					if (mode == TrackedDeviceLoc::TrackedDeviceOnHands) {
						return d->isTrackedAsCNTRL;
					}
					else if (mode == TrackedDeviceLoc::TrackedDeviceOnFeet) {
						return d->isTrackedAsTRKR;
					}
				}
				classIdx++;
			}
		}
		/*if (currentProfileIdx >= 0 && currentProfileIdx < walkInPlaceProfiles.size()) {
			WalkInPlaceProfile* profile = &walkInPlaceProfiles[currentProfileIdx];
			if (mode == 0 && ((profile->cntrl1Idx < 0 && devIdx == 0) || (profile->cntrl2Idx < 0 && devIdx == 1))) {
				return true;
			}
			else if ((mode == 0 && ((devIdx == profile->cntrl1Idx && devClass == profile->cntrl1Type)
				|| (devIdx == profile->cntrl2Idx && devClass == profile->cntrl2Type)))
				|| (mode == 1 && ((devIdx == profile->trkr1Idx && devClass == profile->trkr1Type)
					|| (devIdx == profile->trkr2Idx && devClass == profile->trkr2Type)))) {
				return true;
			}
		}
		else if ((mode == 0 && (devIdx == 0 || devIdx == 1))) {
			return true;
		}*/
		return false;
	}

	void WalkInPlaceTabController::setDirectionDevice(int choice) {
		//["None, Use Game Default","1st controller", "2nd controller", "1st tracker", "2nd tracker", "3rd controller", "4th controller", "3rd tracker", "4th tracker"]
		switch (choice) {
		case 0:
		case 1:
			directionDevice = vr::k_unTrackedDeviceIndexInvalid;
			break;
		case 2:
		case 6:
			directionDevice = controller1ID;
			break;
		case 3:
		case 7:
			directionDevice = controller2ID;
			break;
		case 4:
		case 8:
			directionDevice = tracker1ID;
			break;
		case 5:
		case 9:
			directionDevice = tracker2ID;
			break;
		default:
			break;
		}
	}

	void WalkInPlaceTabController::enableDevice(int deviceClass, int devIdx, bool enable, int mode) {
		int deviceClassI = 0;
		int idx = 0;
		uint64_t foundDevIdx = -1;
		for (auto dev : deviceInfos) {
			if (dev->deviceClass == deviceClass) {
				if (deviceClassI == devIdx) {
					if (mode == TrackedDeviceLoc::TrackedDeviceOnHands) {
						if (enable) {
							if (controller1ID == vr::k_unTrackedDeviceIndexInvalid) {
								controller1ID = dev->openvrId;
								dev->isTrackedAsCNTRL = true;
								foundDevIdx = idx;
							}
							else if (controller2ID == vr::k_unTrackedDeviceIndexInvalid) {
								controller2ID = dev->openvrId;
								dev->isTrackedAsCNTRL = true;
								foundDevIdx = idx;
							}
						}
						else if (!enable) {
							if (controller1ID == dev->openvrId) {
								controller1ID = vr::k_unTrackedDeviceIndexInvalid;
								dev->isTrackedAsCNTRL = false;
								foundDevIdx = idx;
							}
							if (controller2ID == dev->openvrId) {
								controller2ID = vr::k_unTrackedDeviceIndexInvalid;
								dev->isTrackedAsCNTRL = false;
								initializedProfile = true;
								foundDevIdx = idx;
							}
						}
					}
					else if (mode == TrackedDeviceLoc::TrackedDeviceOnFeet) {
						if (enable) {
							if (tracker1ID == vr::k_unTrackedDeviceIndexInvalid) {
								tracker1ID = dev->openvrId;
								dev->isTrackedAsTRKR = true;
								foundDevIdx = idx;
							}
							else if (tracker2ID == vr::k_unTrackedDeviceIndexInvalid) {
								tracker2ID = dev->openvrId;
								dev->isTrackedAsTRKR = true;
								foundDevIdx = idx;
							}
						}
						else if (!enable) {
							if (tracker1ID == dev->openvrId) {
								tracker1ID = vr::k_unTrackedDeviceIndexInvalid;
								dev->isTrackedAsTRKR = false;
								foundDevIdx = idx;
								if (tracker2ID != vr::k_unTrackedDeviceIndexInvalid) {
									tracker1ID = tracker2ID;
									dev->isTrackedAsTRKR = true;
									tracker2ID = vr::k_unTrackedDeviceIndexInvalid;
								}
							}
							if (tracker2ID == dev->openvrId) {
								tracker2ID = vr::k_unTrackedDeviceIndexInvalid;
								dev->isTrackedAsTRKR = false;
								foundDevIdx = idx;
							}
						}
					}
					else {
						LOG(INFO) << "unknown enabled tracked device location: " << mode;
					}
				}
				deviceClassI++;
			}
			idx++;
		}
		if (foundDevIdx >= 0) {
			if (enable 
				&& (identifyDeviceQ.empty() 
				|| (identifyDeviceQ.front() != foundDevIdx 
					&& identifyDeviceQ.back() != foundDevIdx))) 
			{
				identifyDeviceQ.push(foundDevIdx);
				identifyDeviceLast = std::chrono::duration_cast <std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			}
		}
	}

	void WalkInPlaceTabController::setGameType(int type) {
		if (type < 6) {
			gameType->useAxis = true;
			gameType->useClick = !(type == 1 || type == 4);
			gameType->alwaysHeld = (type == 2 || type == 5);
			gameType->inputType = (type < 3) ? InputType::touchpad : InputType::joystick;
		}
		else {
			gameType->useAxis = false;
			gameType->useClick = true;
			gameType->alwaysHeld = false;
			if (type == 6) {
				gameType->inputType = InputType::grip;
			}
			else if (type == 7) {
				gameType->inputType = InputType::keyWASD;
			}
			else if (type == 8) {
				gameType->inputType = InputType::keyArrow;
			}
		}
	}

	void WalkInPlaceTabController::setHMDType(int type) {
		hmdType = type;
	}

	void WalkInPlaceTabController::setPaceControl(int type) {
		switch (type) {
		case 0:
			paceControl = PaceControlType::AvgControllerModelMatch;
			break;
		case 1:
			paceControl = PaceControlType::AvgHMDModelMatch;
			break;
		case 2:
			paceControl = PaceControlType::AvgControllerHMDModelMatch;
			break;
		case 3:
			paceControl = PaceControlType::MinMaxControllerSpeed;
			break;
		case 4:
			paceControl = PaceControlType::CurControllerModelMatch;
			break;
		case 5:
			paceControl = PaceControlType::CurHMDModelMatch;
			break;
		default:
			break;
		}
	}

	void WalkInPlaceTabController::identifyDevice(unsigned deviceIndex, unsigned short duration) {
		if (deviceIndex < deviceInfos.size()) {
			try {
				vr::VRSystem()->TriggerHapticPulse(deviceIndex, 0, duration);
				timeLastHaptic = std::chrono::duration_cast <std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			}
			catch (std::exception& e) {
				LOG(INFO) << "Exception caught while identifying device: " << e.what();
			}
		}
	}

	void WalkInPlaceTabController::runSampleOnModel() {
		if (controller1ID != vr::k_unTrackedDeviceIndexInvalid) {
			auto now = std::chrono::duration_cast <std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			double tdiff = ((double)(now - timeLastTick));
			bool hmdStep = false;
			//LOG(INFO) << "DT: " << tdiff;
			if (tdiff >= dT) {
				timeLastTick = now;
				vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, latestDevicePoses, vr::k_unMaxTrackedDeviceCount);
				testHMDSample(tdiff);
				testTRKRSample(tdiff);
			}
			if (validSample) {
				vr::VRControllerAxis_t axisState;
				determineCurDirection();
				if (gameType->useAxis) {
					axisState.x = 0;
					float tempTouch = determineInputScale();
					if (sNValidTouch != tempTouch) {
						sNValidTouch = tempTouch;
						if (sNValidTouch > 0.5) {
							float normTouch = (sNValidTouch - 0.5) / 0.5;
							axisState.y = (normTouch * (maxTouch - midTouch)) + midTouch;
						}
						else if (sNValidTouch < 0.501) {
							float normTouch = (sNValidTouch / 0.5);
							axisState.y = (normTouch * (midTouch - minTouch)) + minTouch;
						}
						if (false && directionDevice != vr::k_unTrackedDeviceIndexInvalid) {
							axisState.x = minTouch * touchX;
							axisState.y = axisState.y * touchY;
						}
						try {
							if (gameType->useClick && !gameType->alwaysHeld) {
								if (dataModel(TOUCH_VAL_IDX, lastCNTRLSampleMKi) > 0.999) {
									pressedFlag = true;
								}
								else {
									pressedFlag = false;
								}
							}
							applyAxisMovement(axisState);
						}
						catch (std::exception& e) {
							LOG(INFO) << "Exception caught while applying virtual step movement: " << e.what();
						}
					}
				}
				//else if (inputType == 9999) {
				//	applyClickMovement();
				//}
				else if (gameType->inputType == InputType::grip) {
					applyGripMovement();
				}
				if (gameType->inputType == InputType::keyWASD || gameType->inputType == InputType::keyArrow) {
					applyKeyMovement();
				}
			}
		}
	}

	void WalkInPlaceTabController::testHMDSample(double tdiff) {
		vr::HmdVector3d_t hmdVel = { 0, 0, 0 };
		vr::HmdVector3d_t hmdRotVel = { 0, 0, 0 };
		if (trackHMDVel) {
			if (hmdType != 0) {
				auto m = latestDevicePoses[hmdID].mDeviceToAbsoluteTracking.m;

				hmdVel.v[0] = (m[0][3] - lastHmdPos.v[0]) / tdiff;
				hmdVel.v[1] = (m[1][3] - lastHmdPos.v[1]) / tdiff;
				hmdVel.v[2] = (m[2][3] - lastHmdPos.v[2]) / tdiff;

				lastHmdPos.v[0] = m[0][3];
				lastHmdPos.v[1] = m[1][3];
				lastHmdPos.v[2] = m[2][3];

			}
			else {
				hmdVel.v[0] = latestDevicePoses[hmdID].vVelocity.v[0];
				hmdVel.v[1] = latestDevicePoses[hmdID].vVelocity.v[1];
				hmdVel.v[2] = latestDevicePoses[hmdID].vVelocity.v[2];
			}

			vr::HmdQuaternion_t qRotation = vrmath::quaternionFromRotationMatrix(latestDevicePoses[hmdID].mDeviceToAbsoluteTracking);
			vr::HmdVector3d_t forward = { 0,0,-1 };
			vr::HmdVector3d_t right = { 1,0,0 };
			vr::HmdVector3d_t devForward = vrmath::quaternionRotateVector(qRotation, forward);
			vr::HmdVector3d_t devRight = vrmath::quaternionRotateVector(qRotation, right);

			//hmdVel = vrmath::quaternionRotateVector(qRotation, hmdVel);

			double devYaw = (180 * std::asin(devForward.v[0])) / M_PI;
			double devPitch = (180 * std::asin(devForward.v[1])) / M_PI;
			double devRoll = (180 * std::asin(devRight.v[1])) / M_PI;

			hmdRotVel.v[0] = (devYaw - lastHmdRot.v[0]) / tdiff;
			hmdRotVel.v[1] = (devPitch - lastHmdRot.v[1]) / tdiff;
			hmdRotVel.v[2] = (devRoll - lastHmdRot.v[2]) / tdiff;

			lastHmdRot.v[0] = devYaw;
			lastHmdRot.v[1] = devPitch;
			lastHmdRot.v[2] = devRoll;
			try {
				//check rotation and XZ velocity are within threshold
				bool hmdValidRot = !trackHMDRot || (std::abs(hmdRotVel.v[1]) < hmdMaxPROTVel && std::abs(hmdRotVel.v[0]) < hmdMaxYROTVel);
				bool hmdValidXZVel = std::abs(hmdVel.v[0]) < hmdMaxXVel && std::abs(hmdVel.v[2]) < hmdMaxZVel;
				if (!hmdValidRot || !hmdValidXZVel) {
					stopMovement();
				}
				else {
					//build and check Y Vel sample on model
					int n = hmdSample.n_cols;
					hmdSample.insert_cols(n, 1);
					if (n - 2 > maxSNHMD) {
						hmdSample.shed_col(0);
						n = n - 1;
					}
					hmdSample(0, n) = hmdVel.v[1];
					int sNk = validSample ? reqSNHMD : startSNHMD;
					if (hmdSample.n_cols - 1 >= sNk) {
						arma::rowvec mN = arma::abs(dataModel.row(HMD_Y_VEL_IDX));
						arma::rowvec sN = arma::abs(hmdSample.row(0)); // Y VEL
						arma::rowvec lastSN = arma::abs(sN.tail_cols(sNk));
						if (lastSN.max() < minHMDPeakVal) {
							stopMovement();
							lastValidHMDSampleMKi = 0;
						}
						else {
							std::pair<float, int> mDs = computeSNDelta(lastSN, mN);
							if (!validSample) {
								std::pair<int, int> mDv = computeSNDV(lastSN, mN);
								if (mDv.first >= std::floor(hmdMinDVPerSN*sNk) && mDs.first < hmdVelVariance*sNk) {
									validSample = true;
									lastValidHMDSampleMKi = mDs.second;
								}
							}
							/*else if (dataModel(TOUCH_VAL_IDX, mDs.second) < -1) {
								if (mDs.first < hmdVelVariance*sNk) {
									validSample--;
									if (validSample <= 0) {
										validSample = false;
										lastValidHMDSampleMKi = mDs.second;
										stopMovement();
									}
								}
							}*/
							else {
								//validSample++;
								//validSample = validSample > reqSNHMD ? reqSNHMD : validSample;
								lastValidHMDSampleMKi = mDs.second;
							}
						}
					}
				}
			}
			catch (std::exception& e) {
				LOG(INFO) << "Exception caught while computing delta hmd: " << e.what();
			}
		}
	}

	void WalkInPlaceTabController::testTRKRSample(double tdiff) {
		if (tracker1ID != vr::k_unTrackedDeviceIndexInvalid) {
			vr::HmdVector3d_t tracker1Vel = { 0, 0, 0 };
			vr::HmdVector3d_t tracker2Vel = { 0, 0, 0 };
			tracker1Vel.v[0] = latestDevicePoses[tracker1ID].vVelocity.v[0];
			tracker1Vel.v[1] = latestDevicePoses[tracker1ID].vVelocity.v[1];
			tracker1Vel.v[2] = latestDevicePoses[tracker1ID].vVelocity.v[2];

			if (tracker2ID != vr::k_unTrackedDeviceIndexInvalid) {
				tracker2Vel.v[0] = latestDevicePoses[tracker2ID].vVelocity.v[0];
				tracker2Vel.v[1] = latestDevicePoses[tracker2ID].vVelocity.v[1];
				tracker2Vel.v[2] = latestDevicePoses[tracker2ID].vVelocity.v[2];
			}
			try {
				//check tracker vel within threshold
				//bool trkrValidXZVel = std::abs(tracker1Vel.v[0]) < hmdMaxXVel && std::abs(tracker1Vel.v[2]) < hmdMaxZVel;
				//if (!trkrValidXZVel) {
				//	stopMovement();
				//}
				//build and check tracker Y vel sample on model
				int n = trkrSample.n_cols;
				trkrSample.insert_cols(n, 1);
				if (n > maxSNTRKR) {
					trkrSample.shed_col(0);
					n = n - 1;
				}
				trkrSample(0, n) = tracker1Vel.v[1];
				trkrSample(1, n) = tracker2Vel.v[1];
				int sNk = validSample ? reqSNTRKR : startSNTRKR;
				if (trkrSample.n_cols >= sNk) {
					arma::rowvec mN = arma::abs(dataModel.row(TRKR1_Y_VEL_IDX));
					arma::rowvec sN = arma::abs(trkrSample.row(0));
					arma::rowvec lastSN = sN.tail_cols(sNk);
					double trkrMax = lastSN.max();
					if (tracker2ID == vr::k_unTrackedDeviceIndexInvalid && trkrMax < minTRKRPeakVal) {
						stopMovement();
						lastValidTRKRSampleMKi = 0;
					}
					else {
						std::pair<float, int> mDs1 = computeSNDelta(lastSN, mN);
						if (tracker2ID != vr::k_unTrackedDeviceIndexInvalid) {
							sN = arma::abs(trkrSample.row(1)); // trkr 2
							arma::rowvec lastSN2 = sN.tail_cols(sNk);
							trkrMax = std::max(trkrMax, lastSN2.max());
							if (trkrMax < minTRKRPeakVal) {
								stopMovement();
								lastValidTRKRSampleMKi = 0;
							}
							else {
								std::pair<float, int> mDs2 = computeSNDelta(lastSN2, mN);
								if (trkrMax >= maxTRKRPeakVal || (mDs1.first < trkrVariance*sNk && mDs2.first < trkrVariance*sNk)) {
									if (!trackHMDVel && !validSample) {
										validSample = true;
									}
									lastValidTRKRSampleMKi = mDs1.first < mDs2.first ? mDs1.second : mDs2.second;
								}
								else {
									stopMovement();
								}
							}
						}
						else if (trkrMax >= maxTRKRPeakVal || mDs1.first < trkrVariance*sNk) {
							if (!trackHMDVel && !validSample) {
								validSample = true;
							}
							lastValidTRKRSampleMKi = mDs1.second;
						}
						else {
							stopMovement();
						}
					}
				}
			}
			catch (std::exception& e) {
				LOG(INFO) << "Exception caught while computing delta error trackers: " << e.what();
			}
		}
	}

	float WalkInPlaceTabController::determineInputScale() {
		float nextTouch = 0;
		try {
			if (paceControl == PaceControlType::AvgControllerModelMatch
				|| paceControl == PaceControlType::AvgControllerHMDModelMatch
				|| paceControl == PaceControlType::CurControllerModelMatch
				|| paceControl == PaceControlType::MinMaxControllerSpeed) {
				vr::HmdVector3d_t hmdVel = { 0, 0, 0 };
				vr::HmdVector3d_t cont1Vel = { 0, 0, 0 };
				vr::HmdVector3d_t cont2Vel = { 0, 0, 0 };
				cont1Vel.v[0] = latestDevicePoses[controller1ID].vVelocity.v[0];
				cont1Vel.v[1] = latestDevicePoses[controller1ID].vVelocity.v[1];
				cont1Vel.v[2] = latestDevicePoses[controller1ID].vVelocity.v[2];
				if (controller2ID != vr::k_unTrackedDeviceIndexInvalid) {
					cont2Vel.v[0] = latestDevicePoses[controller2ID].vVelocity.v[0];
					cont2Vel.v[1] = latestDevicePoses[controller2ID].vVelocity.v[1];
					cont2Vel.v[2] = latestDevicePoses[controller2ID].vVelocity.v[2];
				}
				if (paceControl == PaceControlType::MinMaxControllerSpeed) {
					double cntrl1VelY = std::abs(cont1Vel.v[1]);
					double cntrl2VelY = std::abs(cont2Vel.v[1]);
					if (controller2ID != vr::k_unTrackedDeviceIndexInvalid) {
						if (cntrl2VelY > cntrl1VelY) {
							return std::min(1.0, cntrl2VelY / avgCNTRLVal);
						}
					}
					return std::min(1.0, cntrl1VelY / avgCNTRLVal);
				}
				int n = cntrlSample.n_cols;
				cntrlSample.insert_cols(n, 1);
				if (n > maxSNCNTRL + 1) {
					cntrlSample.shed_col(0);
					n = n - 1;
				}
				cntrlSample(0, n) = cont1Vel.v[1];
				if (controller2ID != vr::k_unTrackedDeviceIndexInvalid) {
					cntrlSample(1, n) = cont2Vel.v[1];
				}
				if (cntrlSample.n_cols >= reqSNCNTRL) {
					if (controller1ID == tracker1ID) {
						if (paceControl == PaceControlType::CurControllerModelMatch) {
							nextTouch = dataModel(TOUCH_VAL_IDX, lastValidTRKRSampleMKi);
						}
						else if (paceControl == PaceControlType::AvgControllerHMDModelMatch) {
							nextTouch = (dataModel(TOUCH_VAL_IDX, lastValidTRKRSampleMKi) + dataModel(TOUCH_VAL_IDX, lastValidHMDSampleMKi)) / 2.0;
							if (controller2ID != tracker2ID && controller2ID != vr::k_unTrackedDeviceIndexInvalid) {
								arma::rowvec mN = arma::abs(dataModel.row(CNTRL2_Y_VEL_IDX));
								arma::rowvec sN = arma::abs(cntrlSample.row(0));
								arma::rowvec lastSN = sN.tail_cols(reqSNCNTRL);
								std::pair<float, int> mDs2 = computeSNDelta(lastSN, mN);
								float avgCntrlTouch = (dataModel(TOUCH_VAL_IDX, lastValidTRKRSampleMKi) + dataModel(TOUCH_VAL_IDX, mDs2.second)) / 2.0;
								nextTouch = (avgCntrlTouch + dataModel(TOUCH_VAL_IDX, lastValidHMDSampleMKi)) / 2.0;
							}
						}
					}
					else {
						arma::rowvec mN = arma::abs(dataModel.row(CNTRL1_Y_VEL_IDX));
						arma::rowvec sN = arma::abs(cntrlSample.row(0));
						arma::rowvec lastSN = sN.tail_cols(std::min(n, reqSNCNTRL));
						if (paceControl == PaceControlType::CurControllerModelMatch) {
							std::pair<float, int> mDs = computeSNDelta(lastSN, mN);
							nextTouch = dataModel(TOUCH_VAL_IDX, mDs.second);
						}
						else if (paceControl == PaceControlType::AvgControllerHMDModelMatch) {
							std::pair<float, int> mDs1 = computeSNDelta(lastSN, mN);
							nextTouch = (dataModel(TOUCH_VAL_IDX, mDs1.second) + dataModel(TOUCH_VAL_IDX, lastValidHMDSampleMKi)) / 2.0;
							if (controller2ID != vr::k_unTrackedDeviceIndexInvalid) {
								arma::rowvec mN = arma::abs(dataModel.row(CNTRL2_Y_VEL_IDX));
								arma::rowvec sN = arma::abs(cntrlSample.row(0));
								arma::rowvec lastSN = sN.tail_cols(reqSNCNTRL);
								std::pair<float, int> mDs2 = computeSNDelta(lastSN, mN);
								float avgCntrlTouch = (dataModel(TOUCH_VAL_IDX, mDs1.second) + dataModel(TOUCH_VAL_IDX, mDs2.second)) / 2.0;
								nextTouch = (avgCntrlTouch + dataModel(TOUCH_VAL_IDX, lastValidHMDSampleMKi)) / 2.0;
							}
						}
					}
					arma::rowvec cntrl1 = arma::abs(cntrlSample.row(0));
					arma::rowvec cntrl2;
					if (controller2ID != vr::k_unTrackedDeviceIndexInvalid) {
						cntrl2 = arma::abs(cntrlSample.row(1));
					}
					if (paceControl == PaceControlType::AvgControllerModelMatch
						|| paceControl == PaceControlType::AvgControllerHMDModelMatch) {
						if (arma::sum(cntrl1) < cntrlVariance*cntrlSample.n_cols) {
							if (controller2ID == vr::k_unTrackedDeviceIndexInvalid || arma::sum(cntrl2) < cntrlVariance*cntrlSample.n_cols) {
								lastCNTRLSampleMKi = 0;
							}
						}
						else {
							float max = cntrl1.max();
							int nextGTIdx_1 = findMNIdxGTS(max, modelCNTRL1);
							lastCNTRLSampleMKi = nextGTIdx_1;
							if (controller2ID != vr::k_unTrackedDeviceIndexInvalid) {
								float maxB = cntrl2.max();
								int nextGTIdx_2 = findMNIdxGTS(maxB, modelCNTRL2);
								lastCNTRLSampleMKi = nextGTIdx_1 < nextGTIdx_2 ? nextGTIdx_1 : nextGTIdx_2;
							}
						}
						nextTouch = dataModel(TOUCH_VAL_IDX, lastCNTRLSampleMKi);
						if (paceControl == PaceControlType::AvgControllerHMDModelMatch) {
							nextTouch = (nextTouch + dataModel(TOUCH_VAL_IDX, lastValidHMDSampleMKi)) / 2.0;
						}
					}
				}
			}
			else if (paceControl == PaceControlType::CurHMDModelMatch) {
				nextTouch = dataModel(TOUCH_VAL_IDX, lastValidHMDSampleMKi);
			}
			else if (paceControl == PaceControlType::AvgHMDModelMatch) {
				nextTouch = dataModel(TOUCH_VAL_IDX, lastValidHMDSampleMKi);
			}
			float temp = sNValidTouch;
			//sNValidTouch = nextTouch;
			if (nextTouch >= temp) {
				temp = temp + (timeStep*(nextTouch - temp)*2.0);
			}
			else {
				temp = temp - (timeStep*(temp - nextTouch)*6.0);
			}
			return temp;
		}
		catch (std::exception& e) {
			LOG(INFO) << "Exception caught while parsing controller sample: " << e.what();
		}
		return nextTouch;
	}

	void WalkInPlaceTabController::determineCurDirection() {
		if (false && directionDevice != vr::k_unTrackedDeviceIndexInvalid) {
			vr::HmdQuaternion_t qRotation = vrmath::quaternionFromRotationMatrix(latestDevicePoses[directionDevice].mDeviceToAbsoluteTracking);
			vr::HmdVector3d_t forward = { 0,0,-1 };
			vr::HmdVector3d_t right = { 1,0,0 };
			vr::HmdVector3d_t forwardRot = vrmath::quaternionRotateVector(qRotation, forward);
			vr::HmdVector3d_t rightRot = vrmath::quaternionRotateVector(qRotation, right);
			float pitch = (180 * std::asin(forwardRot.v[1])) / M_PI;
			float yaw = (180 * std::asin(forwardRot.v[0])) / M_PI;
			float roll = (180 * std::asin(rightRot.v[1])) / M_PI;
			touchX = 0;
			touchY = 1;
			float diffYaw = (hmdYaw - yaw);
			if (useContDirForRev && pitch > 77) {
				touchX = 0;
				touchY = -1;
			}
			else if (useContDirForStraf && pitch < 77 && std::fabs(diffYaw) > 30) { //std::fabs(roll) > 45 ) {
				touchX = std::sin((diffYaw*M_PI) / 180.0);
				touchY = std::cos((diffYaw*M_PI) / 180.0);
			}
			//LOG(INFO) << "CONT Pitch,Yaw,Roll : " << pitch << ",(" << hmdYaw << "-" << yaw << ")=" << diffYaw << "," << roll;
			//LOG(INFO) << "CONT Pitch,Yaw,Roll : " << pitch << "," <<  yaw << "," << roll;
			//LOG(INFO) << "Cont Forward (x,y,z): " << forwardRot.v[0] << "," << forwardRot.v[1] << "," << forwardRot.v[2];
			//LOG(INFO) << "HMD  Forward (x,y,z): " << hmdForward.v[0] << "," << hmdForward.v[1] << "," << hmdForward.v[2];
		}
	}

	std::pair<float, int> WalkInPlaceTabController::computeSNDelta(arma::mat sN, arma::mat mN) {
		std::pair<float, int> mS;
		mS.first = 9999;
		mS.second = 0;
		for (int mi = 0; mi < mN.n_cols; mi += sN.n_cols) {
			int sKl = (int)sN.n_cols - 1;
			if ((mi + sKl) < mN.n_cols) {
				arma::rowvec mKi = mN.cols(mi, mi + sKl);
				arma::rowvec dKi = arma::abs(mKi - sN);
				float dK_i = std::abs(arma::sum(dKi));
				if (dK_i < mS.first) {
					mS.first = dK_i;
					mS.second = mi;
				}
			}
		}
		return mS;
	}

	std::pair<float, int> WalkInPlaceTabController::computeSNDeltaOffset(arma::mat sN, arma::mat mN) {
		std::pair<float, int> mS;
		mS.first = 9999;
		mS.second = 0;
		int ofs = 0;
		for (int mi = 0; mi < mN.n_cols; mi += sN.n_cols) {
			int sKl = (int)sN.n_cols - 1;
			for (ofs = 0; ofs < sKl; ofs += 2) {
				if ((mi + sKl + ofs) < mN.n_cols) {
					arma::rowvec mKi = mN.cols(mi + ofs, mi + sKl + ofs);
					arma::rowvec dKi = arma::abs(mKi - sN);
					float dK_i = std::abs(arma::sum(dKi));
					if (dK_i < mS.first) {
						mS.first = dK_i;
						mS.second = mi;
					}
				}
			}
		}
		return mS;
	}

	std::pair<int, int> WalkInPlaceTabController::computeSNDV(arma::mat sN, arma::mat mN) {
		std::pair<int, int> mS;
		mS.first = 0;
		mS.second = 0;
		for (int mi = 0; mi < mN.n_cols; mi += sN.n_cols) {
			int sKl = (int)sN.n_cols - 1;
			if ((mi + sKl) < mN.n_cols) {
				arma::rowvec mKi = mN.cols(mi, mi + sKl);
				int dVk = 0;
				for (int i = 1; i < sKl; i++) {
					int sDv = (sN(i) - sN(i - 1)) > 0 ? 1 : -1;
					int mDv = (mKi(i) - mKi(i - 1)) > 0 ? 1 : -1;
					dVk = (sDv == mDv) ? dVk + 1 : dVk;
					if (dVk > mS.first) {
						mS.first = dVk;
						mS.second = mi;
					}
				}
			}
		}
		return mS;
	}

	float WalkInPlaceTabController::findMinPeakMN(int starERR, arma::rowvec mN, float lowRange) {
		float min = 9999;
		float lastPeak = 0;
		int lastV = mN(starERR) < 0 ? -1 : 1;
		for (int mi = starERR; mi < mN.n_cols - 4; mi++) {
			int curV = mN(mi) < 0 ? -1 : 1;
			float mV = std::abs(mN(mi));
			lastPeak = mV > lastPeak ? mV : lastPeak;
			if (mi > starERR && lastV != curV && lastPeak > lowRange) {
				min = lastPeak < min ? lastPeak : min;
				lastPeak = 0;
			}
			lastV = curV;
		}
		return min;
	}

	int WalkInPlaceTabController::findMNIdxGTS(float s, arma::mat mN) {
		for (int mi = 0; mi < mN.n_cols; mi++) {
			if (mN(mi) > s) {
				return mi;
			}
		}
		return mN.n_cols - 1;
	}

	void WalkInPlaceTabController::clearClickedFlag() {
		uint32_t deviceId = ovrwipCNTRLID;
		try {
			if (gameType->useAxis) {
				vr::VRControllerAxis_t axisState;
				axisState.x = 0;
				axisState.y = 0;
				try {
					vrwalkinplace::VRWalkInPlace vrwalkinplace;
					vrwalkinplace.connect();
					if (gameType->inputType == InputType::touchpad) {
						vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonUnpressed, deviceId, vr::k_EButton_SteamVR_Touchpad, 0.0);
						pressedFlag = false;
						vrwalkinplace.openvrAxisEvent(deviceId, vr::k_EButton_SteamVR_Touchpad, axisState);
						vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonUntouched, deviceId, vr::k_EButton_SteamVR_Touchpad, 0.0);
					}
					else {
						vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonUnpressed, deviceId, vr::k_EButton_IndexController_JoyStick, 0.0);
						pressedFlag = false;
						vrwalkinplace.openvrAxisEvent(deviceId, vr::k_EButton_IndexController_JoyStick, axisState);
						vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonUntouched, deviceId, vr::k_EButton_IndexController_JoyStick, 0.0);
					}
				}
				catch (std::exception& e) {
					//LOG(INFO) << "Exception caught while stopping virtual step movement: " << e.what();
				}
			}
		}
		catch (std::exception& e) {
			//LOG(INFO) << "Exception caught while applying virtual axis movement: " << e.what();
		}
	}

	void WalkInPlaceTabController::stopMovement() {
		try {
			if (validSample) {
				stopCallCount = 0;
			}
			if (stopCallCount < 20) {
				stopCallCount++;
				uint32_t deviceId = ovrwipCNTRLID;
				if (gameType->useAxis) {
					vr::VRControllerAxis_t axisState;
					axisState.x = 0;
					axisState.y = 0;
					try {
						vrwalkinplace::VRWalkInPlace vrwalkinplace;
						vrwalkinplace.connect();
						if (gameType->inputType == InputType::touchpad) {
							if (gameType->useClick) {
								vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonUnpressed, deviceId, vr::k_EButton_SteamVR_Touchpad, 0.0);
								pressedFlag = false;
							}
							vrwalkinplace.openvrAxisEvent(deviceId, vr::k_EButton_SteamVR_Touchpad, axisState);
							vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonUntouched, deviceId, vr::k_EButton_SteamVR_Touchpad, 0.0);
						}
						else {
							if (gameType->useClick) {
								vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonUnpressed, deviceId, vr::k_EButton_IndexController_JoyStick, 0.0);
								pressedFlag = false;
							}
							vrwalkinplace.openvrAxisEvent(deviceId, vr::k_EButton_IndexController_JoyStick, axisState);
							vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonUntouched, deviceId, vr::k_EButton_IndexController_JoyStick, 0.0);

						}
					}
					catch (std::exception& e) {
						//LOG(INFO) << "Exception caught while stopping virtual step movement: " << e.what();
					}
				}
				else if (gameType->inputType == InputType::grip) {
					try {
						vrwalkinplace::VRWalkInPlace vrwalkinplace;
						vrwalkinplace.connect();
						vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonUnpressed, deviceId, vr::k_EButton_Grip, 0.0);
					}
					catch (std::exception& e) {
						//LOG(INFO) << "Exception caught while applying virtual grip movement: " << e.what();
					}
				}
				/*else if (gameType == 9999) { //click only disabled atm
					try {
						vrwalkinplace::VRWalkInPlace vrwalkinplace;
						vrwalkinplace.connect();
						vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonUnpressed, deviceId, vr::k_EButton_SteamVR_Touchpad, 0.0);
					}
					catch (std::exception& e) {
						//LOG(INFO) << "Exception caught while stopping virtual teleport movement: " << e.what();
					}
				}*/
				else if (gameType->inputType == InputType::keyWASD) {
#if defined (_WIN64) || !defined (_LP64)
					INPUT input[2];
					input[0].type = INPUT_KEYBOARD;
					input[0].ki.wVk = 0;
					input[0].ki.wScan = MapVirtualKey(0x57, 0);
					input[0].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
					input[0].ki.time = 0;
					input[0].ki.dwExtraInfo = 0;
					SendInput(2, input, sizeof(INPUT));
#else
#endif
				}
				else if (gameType->inputType == InputType::keyArrow) {
#if defined (_WIN64) || !defined (_LP64)
					INPUT input[2];
					input[0].type = INPUT_KEYBOARD;
					input[0].ki.wVk = 0;
					input[0].ki.wScan = MapVirtualKey(0x26, 0);
					input[0].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
					input[0].ki.time = 0;
					input[0].ki.dwExtraInfo = 0;
					SendInput(2, input, sizeof(INPUT));
#else
#endif
				}
			}
			validSample = false;
			if (trackHMDVel && hmdSample.n_cols > 1) {
				hmdSample.shed_cols(0, hmdSample.n_cols - 1);
				if (hmdVels.n_cols > 1) {
					hmdVels.shed_cols(0, hmdVels.n_cols - 1);
				}
			}
			if (trkrSample.n_cols > 1) {
				trkrSample.shed_cols(0, trkrSample.n_cols - 1);
				if (trkrVels.n_cols > 1) {
					trkrVels.shed_cols(0, trkrVels.n_cols - 1);
				}
			}
			if (cntrlSample.n_cols > 1) {
				cntrlSample.shed_cols(0, cntrlSample.n_cols - 1);
			}
			if (dataModel(TOUCH_VAL_IDX, lastValidHMDSampleMKi) > -1) {
				lastValidHMDSampleMKi = 0;
			}
			lastValidHMDSampleMKi = 0;
			lastValidTRKRSampleMKi = 0;
			lastCNTRLSampleMKi = 0;
			sNValidTouch = 0;
		}
		catch (std::exception &e) {
			LOG(INFO) << "error when attempting to stop movement: " << e.what();
		}
	}

	void WalkInPlaceTabController::stopClickMovement() {
		uint32_t deviceId = ovrwipCNTRLID;
		if (gameType == 0) {
			try {
				vrwalkinplace::VRWalkInPlace vrwalkinplace;
				vrwalkinplace.connect();
				vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonUnpressed, deviceId, vr::k_EButton_SteamVR_Touchpad, 0.0);
				pressedFlag = true;
			}
			catch (std::exception& e) {
				//LOG(INFO) << "Exception caught while stopping virtual step movement: " << e.what();
			}
		}
	}

	void WalkInPlaceTabController::applyVirtualStep(float y) {
		uint32_t deviceId = ovrwipCNTRLID;
		vr::VRControllerAxis_t axisState;
		axisState.x = 0;
		axisState.y = y;
		applyAxisMovement(axisState);
	}

	void WalkInPlaceTabController::applyAxisMovement(vr::VRControllerAxis_t axisState) {
		uint32_t deviceId = ovrwipCNTRLID;
		try {
			vrwalkinplace::VRWalkInPlace vrwalkinplace;
			vrwalkinplace.connect();
			if (gameType->inputType == InputType::touchpad) {
				vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonTouched, deviceId, vr::k_EButton_SteamVR_Touchpad, 0.0);
				vrwalkinplace.openvrAxisEvent(deviceId, vr::k_EButton_SteamVR_Touchpad, axisState);
			}
			else {
				vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonTouched, deviceId, vr::k_EButton_IndexController_JoyStick, 0.0);
				vrwalkinplace.openvrAxisEvent(deviceId, vr::k_EButton_IndexController_JoyStick, axisState);
			}
			if (gameType->useClick) {
				if (pressedFlag || gameType->alwaysHeld) {
					if (gameType->inputType == InputType::touchpad) {
						vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonPressed, deviceId, vr::k_EButton_SteamVR_Touchpad, 0.0);
					}
					else {
						vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonPressed, deviceId, vr::k_EButton_IndexController_JoyStick, 0.0);
					}
				}
				else {
					if (gameType->inputType == InputType::touchpad) {
						vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonUnpressed, deviceId, vr::k_EButton_SteamVR_Touchpad, 0.0);
					}
					else {
						vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonUnpressed, deviceId, vr::k_EButton_IndexController_JoyStick, 0.0);
					}
				}

			}
		}
		catch (std::exception& e) {
			//LOG(INFO) << "Exception caught while applying virtual axis movement: " << e.what();
		}
	}

	void WalkInPlaceTabController::applyClickMovement() {
		uint32_t deviceId = ovrwipCNTRLID;
		if (pressedFlag) {
			try {
				vrwalkinplace::VRWalkInPlace vrwalkinplace;
				vrwalkinplace.connect();
				vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonPressed, deviceId, vr::k_EButton_SteamVR_Touchpad, 0.0);
				pressedFlag = false;
			}
			catch (std::exception& e) {
				//LOG(INFO) << "Exception caught while applying virtual telport movement: " << e.what();
			}
		}
		else {
			try {
				vrwalkinplace::VRWalkInPlace vrwalkinplace;
				vrwalkinplace.connect();
				vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonUnpressed, deviceId, vr::k_EButton_SteamVR_Touchpad, 0.0);
				pressedFlag = true;
			}
			catch (std::exception& e) {
				//LOG(INFO) << "Exception caught while resetting virtual telport movement: " << e.what();
			}
		}
	}

	void WalkInPlaceTabController::applyGripMovement() {
		uint32_t deviceId = ovrwipCNTRLID;
		try {
			vrwalkinplace::VRWalkInPlace vrwalkinplace;
			vrwalkinplace.connect();
			vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonPressed, deviceId, vr::k_EButton_Grip, 0.0);
		}
		catch (std::exception& e) {
			//LOG(INFO) << "Exception caught while applying virtual telport movement: " << e.what();
		}
	}

	void WalkInPlaceTabController::applyKeyMovement() {
		if (gameType->inputType == InputType::keyWASD) {
			if (pressedFlag) {
#if defined (_WIN64) || !defined (_LP64)
				INPUT input[2];
				input[0].type = INPUT_KEYBOARD;
				input[0].ki.wVk = 0;
				input[0].ki.wScan = MapVirtualKey(0x57, 0);
				input[0].ki.dwFlags = KEYEVENTF_SCANCODE;
				input[0].ki.time = 0;
				input[0].ki.dwExtraInfo = 0;
				SendInput(1, input, sizeof(INPUT));
#else
#endif
			}
			pressedFlag = false;
		}
		else if (gameType->inputType == InputType::keyArrow) {
			if (pressedFlag) {
#if defined (_WIN64) || !defined (_LP64)
				INPUT input[2];
				input[0].type = INPUT_KEYBOARD;
				input[0].ki.wVk = 0;
				input[0].ki.wScan = MapVirtualKey(0x26, 0);
				input[0].ki.dwFlags = KEYEVENTF_SCANCODE;
				input[0].ki.time = 0;
				input[0].ki.dwExtraInfo = 0;
				SendInput(1, input, sizeof(INPUT));
#else
#endif
			}
			pressedFlag = false;
		}
	}

} // namespace walkinplace
