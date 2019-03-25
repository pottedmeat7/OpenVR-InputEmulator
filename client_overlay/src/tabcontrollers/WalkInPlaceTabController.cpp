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
	}


	void WalkInPlaceTabController::initStage2(OverlayController * parent, QQuickWindow * widget) {
		this->parent = parent;
		this->widget = widget;
		try {
			for (uint32_t id = 0; id < vr::k_unMaxTrackedDeviceCount; ++id) {
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
						if (deviceClass == vr::ETrackedDeviceClass::TrackedDeviceClass_HMD) {
							if (hmdID == vr::k_unTrackedDeviceIndexInvalid) {
								hmdID = info->openvrId;
								/*try {
									vrwalkinplace::VRWalkInPlace vrwalkinplace;
									vrwalkinplace.connect();
									vrwalkinplace.openvrDeviceAdded(hmdID);
								}
								catch (std::exception& e) {
									LOG(INFO) << "Exception caught while adding hmd to driver: " << e.what();
								}*/
							}
						}
						else if (deviceClass == vr::ETrackedDeviceClass::TrackedDeviceClass_Controller) {
							if (info->serial.find("ovrwip") == std::string::npos) {
								if (controller1ID == vr::k_unTrackedDeviceIndexInvalid) {
									controller1ID = info->openvrId;
									LOG(INFO) << "Found device: id " << info->openvrId << ", class " << info->deviceClass << ", serial " << info->serial;
								}
								else if (controller2ID == vr::k_unTrackedDeviceIndexInvalid && info->openvrId != controller1ID) {
									controller2ID = info->openvrId;
									LOG(INFO) << "Found device: id " << info->openvrId << ", class " << info->deviceClass << ", serial " << info->serial;
								}
							}
							else {
								LOG(INFO) << "Found device: id " << info->openvrId << ", class " << info->deviceClass << ", serial " << info->serial;
								ovrwipCNTRLID = info->openvrId;
							}
						}
						else if (deviceClass == vr::ETrackedDeviceClass::TrackedDeviceClass_GenericTracker) {
							if (tracker1ID == vr::k_unTrackedDeviceIndexInvalid) {
								tracker1ID = info->openvrId;
								LOG(INFO) << "Found device: id " << info->openvrId << ", class " << info->deviceClass << ", serial " << info->serial;
							}
							else if (tracker2ID == vr::k_unTrackedDeviceIndexInvalid && info->openvrId != tracker1ID) {
								tracker2ID = info->openvrId;
								LOG(INFO) << "Found device: id " << info->openvrId << ", class " << info->deviceClass << ", serial " << info->serial;
							}
						}
					}
					maxValidDeviceId = id;
				}
			}
		}
		catch (const std::exception& e) {
			LOG(ERROR) << "Could not connect to driver component: " << e.what();
		}
	}


	void WalkInPlaceTabController::eventLoopTick() {
		if (!initializedDataModel) {
			if (!dataTrainingRequired) {
				loadDataModel();
			}
		}
		else if (wipEnabled) {
			if (!initializedDriver) {
				if (controller2ID != vr::k_unTrackedDeviceIndexInvalid && controller1ID != vr::k_unTrackedDeviceIndexInvalid) {
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
		if (identifyControlTimerSet) {
			auto now = std::chrono::duration_cast <std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			auto tdiff = ((double)(now - identifyControlLastTime));
			//LOG(INFO) << "DT: " << tdiff;
			if (tdiff >= identifyControlTimeOut) {
				identifyControlTimerSet = false;
				try {
					int model_count = vr::VRRenderModels()->GetRenderModelCount();
					for (int model_index = 0; model_index < model_count; model_index++) {
						char buffer[vr::k_unMaxPropertyStringSize];
						vr::VRRenderModels()->GetRenderModelName(model_index, buffer, vr::k_unMaxPropertyStringSize);
						if ((std::string(buffer).compare("vr_controller_vive_1_5")) == 0) {
							vive_controller_model_index = model_index;
							break;
						}
					}
				}
				catch (std::exception& e) {
					LOG(INFO) << "Exception caught while finding vive controller model: " << e.what();
				}
				setDeviceRenderModel(controlSelectOverlayHandle, 0, 1, 1, 1, 1, 1, 1);
			}
		}
		if (settingsUpdateCounter >= 45) {
			settingsUpdateCounter = 0;
			if (hmdID == vr::k_unTrackedDeviceIndexInvalid ||
				(controller1ID == vr::k_unTrackedDeviceIndexInvalid || controller2ID == vr::k_unTrackedDeviceIndexInvalid) ||
				(useTrackers && (tracker1ID == vr::k_unTrackedDeviceIndexInvalid || tracker2ID == vr::k_unTrackedDeviceIndexInvalid))) {
				bool newDeviceAdded = false;
				for (uint32_t id = maxValidDeviceId + 1; id < vr::k_unMaxTrackedDeviceCount; ++id) {
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
							if (deviceClass == vr::ETrackedDeviceClass::TrackedDeviceClass_HMD) {
								if (hmdID == vr::k_unTrackedDeviceIndexInvalid) {
									hmdID = info->openvrId;
								}
							}
							else if (deviceClass == vr::ETrackedDeviceClass::TrackedDeviceClass_Controller) {
								if (info->serial.find("ovrwip") == std::string::npos) {
									if (controller1ID == vr::k_unTrackedDeviceIndexInvalid) {
										controller1ID = info->openvrId;
										LOG(INFO) << "Found device: id " << info->openvrId << ", class " << info->deviceClass << ", serial " << info->serial;
									}
									else if (controller2ID == vr::k_unTrackedDeviceIndexInvalid && info->openvrId != controller1ID) {
										controller2ID = info->openvrId;
										LOG(INFO) << "Found device: id " << info->openvrId << ", class " << info->deviceClass << ", serial " << info->serial;
									}
								}
								else {
									LOG(INFO) << "Found device: id " << info->openvrId << ", class " << info->deviceClass << ", serial " << info->serial;
									ovrwipCNTRLID = info->openvrId;
								}
							}
							else if (deviceClass == vr::ETrackedDeviceClass::TrackedDeviceClass_GenericTracker) {
								if (tracker1ID == vr::k_unTrackedDeviceIndexInvalid) {
									tracker1ID = info->openvrId;
									LOG(INFO) << "Found device: id " << info->openvrId << ", class " << info->deviceClass << ", serial " << info->serial;
								}
								else if (tracker2ID == vr::k_unTrackedDeviceIndexInvalid && info->openvrId != tracker1ID) {
									tracker2ID = info->openvrId;
									LOG(INFO) << "Found device: id " << info->openvrId << ", class " << info->deviceClass << ", serial " << info->serial;
								}
							}
						}
						maxValidDeviceId = id;
					}
				}
			}
		}
		else {
			settingsUpdateCounter++;
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
		return gameType;
	}

	int WalkInPlaceTabController::getHMDType() {
		return hmdType;
	}

	int WalkInPlaceTabController::getButtonControlSelect() {
		return buttonControlSelect;
	}

	int WalkInPlaceTabController::getDisableButton() {
		return disableButton;
	}

	bool WalkInPlaceTabController::getUseButtonAsToggle() {
		return buttonAsToggle;
	}

	bool WalkInPlaceTabController::getButtonEnables() {
		return buttonEnables;
	}

	bool WalkInPlaceTabController::getUseTrackers() {
		return useTrackers;
	}

	bool WalkInPlaceTabController::getDisableHMD() {
		return trackHMDVel;
	}

	float WalkInPlaceTabController::getWalkTouch() {
		return minTouch;
	}

	float WalkInPlaceTabController::getJogTouch() {
		return midTouch;
	}

	float WalkInPlaceTabController::getRunTouch() {
		return maxTouch;
	}

	bool WalkInPlaceTabController::getUseContDirForStraf() {
		return useContDirForStraf;
	}

	bool WalkInPlaceTabController::getUseContDirForRev() {
		return useContDirForRev;
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
				arma::rowvec mCNTRL1 = arma::abs(dataModel.row(CNTRL1_Y_VEL_IDX));
				arma::rowvec mCNTRL2 = arma::abs(dataModel.row(CNTRL2_Y_VEL_IDX));
				mKAVGCNTRL.clear();
				for (int k = 0; k < mCNTRL1.n_cols-1; k+=reqSNCNTRL) {
					arma::rowvec mK1 = mCNTRL1.cols(k, std::min(k + reqSNCNTRL, (int)mCNTRL1.n_cols - 1));
					arma::rowvec mK2 = mCNTRL2.cols(k, std::min(k + reqSNCNTRL, (int)mCNTRL2.n_cols - 1));
					mKAVGCNTRL.insert_cols(0,1);
					mKAVGCNTRL(k) = (arma::mean(mK1) + arma::mean(mK1)) / 2.0;
				}
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
			//addDataModel(QString(currentProfileName.c_str()));
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
		if (!wipEnabled) {
			vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, latestDevicePoses, vr::k_unMaxTrackedDeviceCount);
		}
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
		//LOG(INFO) << "HMD VALS: " << hmdVel.v[0] << "," << hmdVel.v[1] << "," << hmdVel.v[2];	

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
			entry.wipEnabled = settings->value("wipEnabled", false).toBool();
			entry.gameType = settings->value("gameType", 0).toInt();
			entry.hmdType = settings->value("hmdType", 0).toInt();
			entry.buttonControlSelect = settings->value("buttonControlSelect", 0).toInt();
			entry.useTrackers = settings->value("useTrackers", false).toBool();
			entry.trackHMDVel = settings->value("trackHMDVel", true).toBool();
			entry.trackHMDRot = settings->value("trackHMDRot", true).toBool();
			entry.useContDirForStraf = settings->value("useContDirForStraf", false).toBool();
			entry.useContDirForRev = settings->value("useContDirForRev", false).toBool();
			entry.minInputTime = settings->value("minInputTime", 0.25).toDouble();
			entry.disableButton = settings->value("disableButton", 0).toInt();
			entry.minTouch = settings->value("minTouch", 0.6).toFloat();
			entry.midTouch = settings->value("midTouch", 0.87).toFloat();
			entry.maxTouch = settings->value("maxTouch", 1).toFloat();
			entry.buttonAsToggle = settings->value("buttonAsToggle", false).toBool();
			entry.buttonEnables = settings->value("buttonEnables", false).toBool();
			entry.modelFile = settings->value("modelFile", QString((model_file_name + model_file_type).c_str())).toString().toStdString();
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
			settings->setValue("wipEnabled", p.wipEnabled);
			settings->setValue("gameType", p.gameType);
			settings->setValue("hmdType", p.hmdType);
			settings->setValue("buttonControlSelect", p.buttonControlSelect);
			settings->setValue("useTrackers", p.useTrackers);
			settings->setValue("trackHMDVel", p.trackHMDVel);
			settings->setValue("useContDirForStraf", p.useContDirForStraf);
			settings->setValue("useContDirForRev", p.useContDirForRev);
			settings->setValue("minInputTime", p.minInputTime);
			settings->setValue("disableButton", p.disableButton);
			settings->setValue("minTouch", p.minTouch);
			settings->setValue("midTouch", p.midTouch);
			settings->setValue("maxTouch", p.maxTouch);
			settings->setValue("buttonAsToggle", p.buttonAsToggle);
			settings->setValue("buttonEnables", p.buttonEnables);
			settings->setValue("modelFile", QString::fromStdString(p.modelFile));
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
		currentProfileName = name.toStdString();
		profile->wipEnabled = isWIPEnabled();
		profile->gameType = gameType;
		profile->hmdType = hmdType;
		profile->buttonControlSelect = buttonControlSelect;
		profile->useTrackers = useTrackers || !trackHMDVel;
		profile->trackHMDVel = trackHMDVel;
		profile->useContDirForStraf = useContDirForStraf;
		profile->useContDirForRev = useContDirForRev;
		profile->disableButton = disableButton;
		profile->minTouch = minTouch;
		profile->midTouch = midTouch;
		profile->maxTouch = maxTouch;
		profile->buttonAsToggle = buttonAsToggle;
		profile->buttonEnables = buttonEnables;
		saveProfiles();
		OverlayController::appSettings()->sync();
	}

	void WalkInPlaceTabController::applyProfile(unsigned index) {
		if (index < walkInPlaceProfiles.size()) {
			auto& profile = walkInPlaceProfiles[index];
			gameType = profile.gameType;
			hmdType = profile.hmdType;
			buttonControlSelect = profile.buttonControlSelect;
			useTrackers = profile.useTrackers || !profile.trackHMDVel;
			trackHMDVel = profile.trackHMDVel;
			useContDirForStraf = profile.useContDirForStraf;
			useContDirForRev = profile.useContDirForRev;
			disableButton = profile.disableButton;
			minTouch = profile.minTouch;
			midTouch = profile.midTouch;
			maxTouch = profile.maxTouch;
			buttonAsToggle = profile.buttonAsToggle;
			buttonEnables = profile.buttonEnables;

			enableWIP(profile.wipEnabled);
			setGameStepType(profile.gameType);
			setHMDType(profile.hmdType);
			setButtonControlSelect(profile.buttonControlSelect);
			setTrackHMDVel(profile.trackHMDVel);
			setUseContDirForStraf(profile.useContDirForStraf);
			setUseContDirForRev(profile.useContDirForRev);
			setMinInputTime(profile.minInputTime);
			setDisableButton(profile.disableButton);
			setButtonAsToggle(profile.buttonAsToggle);
			setButtonEnables(profile.buttonEnables);
			setMinTouch(profile.minTouch);
			setMidTouch(profile.midTouch);
			setMaxTouch(profile.maxTouch);
		}
	}

	void WalkInPlaceTabController::deleteProfile(unsigned index) {
		if (index < walkInPlaceProfiles.size()) {
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

	void WalkInPlaceTabController::setMinInputTime(double value) {

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

	void WalkInPlaceTabController::setUseContDirForStraf(bool val) {
		useContDirForStraf = val;
	}

	void WalkInPlaceTabController::setUseContDirForRev(bool val) {
		useContDirForRev = val;
	}

	void WalkInPlaceTabController::setTrackHMDVel(bool value) {
		trackHMDVel = value;
		if (!trackHMDVel && !useTrackers) {
			useTrackers = true;
		}
	}

	void WalkInPlaceTabController::setTrackHMDRot(bool value) {
		trackHMDRot = value;
	}

	void WalkInPlaceTabController::setHMDMaxVari(float value) {
		hmdVelVariance = value;

	}

	void WalkInPlaceTabController::setDisableButton(int buttonId) {
		disableButton = buttonId;
	}

	bool WalkInPlaceTabController::getDeviceEnabled(int devClass, int devIdx, int mode) {
		//TODO load dev indicesand class enabled from profile and return state here
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
		int foundDevIdx = -1;
		for (auto dev : deviceInfos) {
			if (dev->deviceClass == deviceClass) {
				if (deviceClassI == devIdx) {
					if (mode == 0) {
						if (enable) {
							if (controller1ID == vr::k_unTrackedDeviceIndexInvalid) {
								controller1ID = dev->openvrId;
								foundDevIdx = idx;
							}
							else if (controller2ID == vr::k_unTrackedDeviceIndexInvalid) {
								controller2ID = dev->openvrId;
								foundDevIdx = idx;
							}
						}
						else if (!enable) {
							if (controller1ID == dev->openvrId) {
								controller1ID = vr::k_unTrackedDeviceIndexInvalid;
								foundDevIdx = idx;
							}
							if (controller2ID == dev->openvrId) {
								controller2ID = vr::k_unTrackedDeviceIndexInvalid;
								foundDevIdx = idx;
							}
						}
					}
					else {
						if (enable) {
							useTrackers = true;
							if (tracker1ID == vr::k_unTrackedDeviceIndexInvalid) {
								tracker1ID = dev->openvrId;
								foundDevIdx = idx;
							}
							else if (tracker2ID == vr::k_unTrackedDeviceIndexInvalid) {
								tracker2ID = dev->openvrId;
								foundDevIdx = idx;
							}
						}
						else if (!enable) {
							if (tracker1ID == dev->openvrId) {
								tracker1ID = vr::k_unTrackedDeviceIndexInvalid;
								foundDevIdx = idx;
							}
							if (tracker2ID == dev->openvrId) {
								tracker2ID = vr::k_unTrackedDeviceIndexInvalid;
								foundDevIdx = idx;
							}
						}
					}
				}
				deviceClassI++;
			}
			idx++;
		}
		if (tracker1ID == vr::k_unTrackedDeviceIndexInvalid && tracker2ID == vr::k_unTrackedDeviceIndexInvalid) {
			useTrackers = false;
		}
		if (foundDevIdx >= 0) {
			identifyControlTimerSet = true;
			identifyControlLastTime = std::chrono::duration_cast <std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			if (enable) {
				setDeviceRenderModel(foundDevIdx, vive_controller_model_index, 0, 1, 0, 1.1, 1.1, 1.1);
			} else {
				setDeviceRenderModel(foundDevIdx, vive_controller_model_index, 1, 0, 0, 1.1, 1.1, 1.1);
			}
		}
	}

	void WalkInPlaceTabController::updateButtonState(uint32_t deviceId, bool firstController) {
		if (deviceId != vr::k_unTrackedDeviceIndexInvalid && (buttonControlSelect >= 2 || (firstController == buttonControlSelect == 0))) {
			vr::VRControllerState_t state;
			vr::VRSystem()->GetControllerState(deviceId, &state, sizeof(state));
			//LOG(INFO) << "Check accuracy button : " << deviceId << " : " << g_AccuracyButton << " : " << state.ulButtonPressed << " : " << vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger);
			//LOG(INFO) << "current button : " << state.ulButtonPressed;
			bool isHoldingButton = false;
			vr::EVRButtonId buttonUsed;
			switch (disableButton) {
			case 0:
				buttonUsed = vr::EVRButtonId::k_EButton_Grip;
				break;
			case 1:
				buttonUsed = vr::EVRButtonId::k_EButton_SteamVR_Trigger;
				break;
			default:
				break;
			}
			switch (buttonUsed) {
			case vr::EVRButtonId::k_EButton_Grip:
				if (state.ulButtonPressed& vr::ButtonMaskFromId(vr::k_EButton_Grip)) {
					isHoldingButton = true;
				}
				break;
			case vr::EVRButtonId::k_EButton_Axis0:
				if (state.ulButtonTouched& vr::ButtonMaskFromId(vr::k_EButton_Axis0)) {
					isHoldingButton = true;
				}
				break;
			case vr::EVRButtonId::k_EButton_SteamVR_Trigger:
				if (state.ulButtonPressed& vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger)) {
					isHoldingButton = true;
				}
				if (state.ulButtonTouched& vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger)) {
					isHoldingButton = true;
				}
				break;
			default:
				break;
			}
			if ((buttonControlSelect == 0 && firstController && isHoldingButton) ||
				(buttonControlSelect == 1 && !firstController && isHoldingButton) ||
				(buttonControlSelect >= 2 && isHoldingButton)) {
				holdingButton = true;
			}
			if ((buttonControlSelect == 0 && firstController && !isHoldingButton) ||
				(buttonControlSelect == 1 && !firstController && !isHoldingButton)) {
				holdingButton = false;
			}
		}
	}

	bool WalkInPlaceTabController::buttonStatus() {
		return (disableButton == 2 || (holdingButton && buttonEnables) || (!holdingButton && !buttonEnables));
	}

	void WalkInPlaceTabController::setButtonAsToggle(bool val) {
		buttonAsToggle = val;
	}

	void WalkInPlaceTabController::setButtonEnables(bool val) {
		buttonEnables = val;
	}

	void WalkInPlaceTabController::setGameStepType(int type) {
		gameType = type;
	}

	void WalkInPlaceTabController::setHMDType(int type) {
		hmdType = type;
	}

	void WalkInPlaceTabController::setDeviceRenderModel(unsigned deviceIndex, unsigned renderModelIndex, float r, float g, float b, float sx, float sy, float sz) {
		if (deviceIndex < deviceInfos.size()) {
			try {
				if (renderModelIndex == 0) {
					if (deviceInfos[deviceIndex]->renderModelOverlay != vr::k_ulOverlayHandleInvalid) {
						vr::VROverlay()->DestroyOverlay(deviceInfos[deviceIndex]->renderModelOverlay);
						deviceInfos[deviceIndex]->renderModelOverlay = vr::k_ulOverlayHandleInvalid;
					}
				}
				else {
					vr::VROverlayHandle_t overlayHandle = deviceInfos[deviceIndex]->renderModelOverlay;
					if (overlayHandle == vr::k_ulOverlayHandleInvalid) {
						std::string overlayName = std::string("RenderModelOverlay_") + std::string(deviceInfos[deviceIndex]->serial);
						auto oerror = vr::VROverlay()->CreateOverlay(overlayName.c_str(), overlayName.c_str(), &overlayHandle);
						if (oerror == vr::VROverlayError_None) {
							overlayHandle = deviceInfos[deviceIndex]->renderModelOverlay = overlayHandle;
						}
						else {
							LOG(INFO) << "Could not create render model overlay: " << vr::VROverlay()->GetOverlayErrorNameFromEnum(oerror);
						}
					}
					if (overlayHandle != vr::k_ulOverlayHandleInvalid) {
						std::string texturePath = QApplication::applicationDirPath().toStdString() + "\\res\\transparent.png";
						if (QFile::exists(QString::fromStdString(texturePath))) {
							vr::VROverlay()->SetOverlayFromFile(overlayHandle, texturePath.c_str());
							char buffer[vr::k_unMaxPropertyStringSize];
							vr::VRRenderModels()->GetRenderModelName(renderModelIndex, buffer, vr::k_unMaxPropertyStringSize);
							vr::VROverlay()->SetOverlayRenderModel(overlayHandle, buffer, nullptr);
							vr::HmdMatrix34_t trans = {
								sx, 0.0f, 0.0f, 0.0f,
								0.0f, sy, 0.0f, 0.0f,
								0.0f, 0.0f, sz, 0.0f
							};
							vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(overlayHandle, deviceInfos[deviceIndex]->openvrId, &trans);
							vr::VROverlay()->ShowOverlay(overlayHandle);
							vr::VROverlay()->SetOverlayColor(overlayHandle, r, g, b);
							identifyControlTimerSet = true;
						}
						else {
							LOG(INFO) << "Could not find texture \"" << texturePath << "\"";
						}
					}
					//LOG(INFO) << "Successfully created control select Overlay for device: " << deviceInfos[deviceIndex]->openvrId << " Index: " << deviceIndex;
				}
			}
			catch (std::exception& e) {
				LOG(INFO) << "Exception caught while updating control select overlay: " << e.what();
			}
		}
	}

	void WalkInPlaceTabController::setButtonControlSelect(int control) {
		buttonControlSelect = control;
		if ((control == 0 && controller1ID != vr::k_unTrackedDeviceIndexInvalid) ||
			(control == 1 && controller2ID != vr::k_unTrackedDeviceIndexInvalid)) {
			if (!identifyControlTimerSet) {
				identifyControlTimerSet = true;
				identifyControlLastTime = std::chrono::duration_cast <std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				controlSelectOverlayHandle = 999;
				for (int d = 0; d < deviceInfos.size(); d++) {
					if (deviceInfos[d]->openvrId == (control == 0 ? controller1ID : controller2ID)) {
						controlSelectOverlayHandle = d;
					}
				}
				try {
					if (vive_controller_model_index < 0) {
						int model_count = vr::VRRenderModels()->GetRenderModelCount();
						for (int model_index = 0; model_index < model_count; model_index++) {
							char buffer[vr::k_unMaxPropertyStringSize];
							vr::VRRenderModels()->GetRenderModelName(model_index, buffer, vr::k_unMaxPropertyStringSize);
							if ((std::string(buffer).compare("vr_controller_vive_1_5")) == 0) {
								vive_controller_model_index = model_index;
								break;
							}
						}
					}
				}
				catch (std::exception& e) {
					LOG(INFO) << "Exception caught while finding vive controller model: " << e.what();
				}
				if (vive_controller_model_index < 0) {
					vive_controller_model_index = 24;
				}
				setDeviceRenderModel(controlSelectOverlayHandle, vive_controller_model_index, 0, 1, 0, 1.1, 1.1, 1.1);
			}
		}
	}

	void WalkInPlaceTabController::runSampleOnModel() {
		if (controller1ID != vr::k_unTrackedDeviceIndexInvalid) {
			double deltatime = 1.0 / 40.0 * 1000;
			auto now = std::chrono::duration_cast <std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			double tdiff = ((double)(now - timeLastTick));
			bool hmdStep = false;
			//LOG(INFO) << "DT: " << tdiff;
			if (tdiff >= deltatime) {
				timeLastTick = now;
				if (disableButton == 0 || disableButton == 1) {
					holdingButton = false;
					updateButtonState(controller1ID, true);
					if (controller2ID != vr::k_unTrackedDeviceIndexInvalid) {
						updateButtonState(controller2ID, false);
					}
				}
				vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, latestDevicePoses, vr::k_unMaxTrackedDeviceCount);
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
						bool hmdValidRot = !trackHMDRot || (std::abs(hmdRotVel.v[1]) < hmdMaxPROTVel && std::abs(hmdRotVel.v[0]) < hmdMaxYROTVel);
						bool hmdValidXZVel = std::abs(hmdVel.v[0]) < hmdMaxXVel && std::abs(hmdVel.v[2]) < hmdMaxZVel;
						if (!hmdValidRot && hmdValidXZVel) {
							stopMovement();
						}
						else {
							int n = hmdSample.n_cols;
							hmdSample.insert_cols(n, 1);
							if (n - 2 > maxSNHMD) {
								hmdSample.shed_col(0);
								n = n - 1;
							}
							hmdSample(0, n) = hmdVel.v[1];
							int sNk = validSample ? reqSNHMD : startSNHMD;
							if (buttonStatus() && hmdSample.n_cols - 1 >= sNk) {
								arma::rowvec mN = arma::abs(dataModel.row(HMD_Y_VEL_IDX));
								arma::rowvec sN = arma::abs(hmdSample.row(0)); // Y VEL
								arma::rowvec lastSN = arma::abs(sN.tail_cols(sNk));
								float lastSN_Sum = arma::sum(lastSN);
								std::pair<float, int> mDs = computeSNDelta(lastSN, mN);
								if (lastSN_Sum <= mDs.first) {
									if (validSample) {
										stopMovement();
									}
									lastValidHMDSampleMKi = mDs.second;
								}
								else {
									if (!validSample) {
										std::pair<int, int> mDv = computeSNDV(lastSN, mN);
										if (mDv.first >= std::floor(hmdMinDVPerSN*sNk) && mDs.first < hmdVelVariance*sNk) {
											validSample = true;
											inputStateChanged = true;
										}
									}
									lastValidHMDSampleMKi = mDs.second;
								}
							}
						}
					}
					catch (std::exception& e) {
						LOG(INFO) << "Exception caught while computing delta hmd: " << e.what();
					}
				}
				if (useTrackers && tracker1ID != vr::k_unTrackedDeviceIndexInvalid) {
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
						int n = trkrSample.n_cols;
						trkrSample.insert_cols(n, 1);
						if (n > maxSNTRKR) {
							trkrSample.shed_col(0);
							n = n - 1;
						}
						trkrSample(0, n) = tracker1Vel.v[1];
						trkrSample(1, n) = tracker2Vel.v[1];
						int sNk = validSample ? reqSNTRKR : startSNTRKR;
						if (buttonStatus() && trkrSample.n_cols >= sNk) {
							arma::rowvec mN = arma::abs(dataModel.row(TRKR1_Y_VEL_IDX));
							arma::rowvec sN = arma::abs(trkrSample.row(0));
							arma::rowvec lastSN = sN.tail_cols(sNk);
							std::pair<float, int> mDs1 = computeSNDelta(lastSN, mN);
							float lastSN_sum = arma::sum(lastSN);
							if (lastSN_sum <= trkrVariance * 2) {
								stopMovement();
							}
							else if (tracker2ID != vr::k_unTrackedDeviceIndexInvalid) {
								sN = arma::abs(trkrSample.row(1)); // trkr 2
								lastSN = sN.tail_cols(sNk);
								std::pair<float, int> mDs2 = computeSNDelta(lastSN, mN);
								float lastSN_sum = arma::sum(lastSN);
								if (lastSN_sum <= trkrVariance * 2) {
									stopMovement();
								}
								else if (mDs1.first < trkrVariance*sNk && mDs2.first < trkrVariance*sNk) {
									if (!trackHMDVel && !validSample) {
										validSample = true;
										inputStateChanged = true;
									}
									lastValidTRKRSampleMKi = mDs1.first < mDs2.first ? mDs1.second : mDs2.second;
								}
							}
							else if (mDs1.first < trkrVariance*sNk) {
								if (!trackHMDVel && !validSample) {
									validSample = true;
									inputStateChanged = true;
								}
								lastValidTRKRSampleMKi = mDs1.second;							
							}
						}
					}
					catch (std::exception& e) {
						LOG(INFO) << "Exception caught while computing delta error trackers: " << e.what();
					}
				}
				timeLastTick = now;
			}
			if (validSample) {
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
				try {
					int n = cntrlSample.n_cols;
					cntrlSample.insert_cols(n, 1);
					if (n > maxSNCNTRL) {
						cntrlSample.shed_col(0);
						n = n - 1;
					}
					cntrlSample(0, n) = cont1Vel.v[1];
					cntrlSample(1, n) = cont2Vel.v[1];

					if ((now)-timeLastCNTRLSN > (deltatime*8)) {
						if (cntrlSample.n_cols >= reqSNCNTRL) {
							arma::rowvec cntrl1 = arma::abs(cntrlSample.row(0));
							arma::rowvec cntrl2 = arma::abs(cntrlSample.row(1));
							int sM1Idx = computeDMean((arma::mean(cntrl1) + arma::mean(cntrl2)) / 2.0, mKAVGCNTRL);
							//arma::rowvec modelCNTRL1 = arma::abs(dataModel.row(0));
							//arma::rowvec modelCNTRL2 = arma::abs(dataModel.row(1));
							//std::pair<float, int> mS1 = computeSNDelta(cntrl1, modelCNTRL1);
							//std::pair<float, int> mS2 = computeSNDelta(cntrl2, modelCNTRL2);
							//std::pair<float, int> mS3 = computeSNDelta(cntrl1, modelCNTRL2);
							//std::pair<float, int> mS4 = computeSNDelta(cntrl2, modelCNTRL1);
							//int temp = mS1.first < mS2.first ? mS1.second : mS2.second;
							
							lastCNTRLSampleMKi = sM1Idx;

							sNValidTouch = dataModel(TOUCH_VAL_IDX, lastCNTRLSampleMKi);
							inputStateChanged = true;
							timeLastCNTRLSN = now;
						}
					}
				}
				catch (std::exception& e) {
					LOG(INFO) << "Exception caught while parsing controller sample: " << e.what();
				}
				if (inputStateChanged) {
					vr::VRControllerAxis_t axisState;
					if (directionDevice != vr::k_unTrackedDeviceIndexInvalid) {
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
					if (gameType == 0 || gameType == 1 || gameType == 2) {
						axisState.x = 0;
						float touch = (maxTouch - minTouch) * sNValidTouch;
						if (sNValidTouch < 0.001) {
							touch = minTouch;
						}
						else if (sNValidTouch > 0.499 && sNValidTouch < 0.501) {
							touch = midTouch;
						}
						axisState.y = touch;
						if (directionDevice != vr::k_unTrackedDeviceIndexInvalid) {
							axisState.x = minTouch * touchX;
							axisState.y = axisState.y * touchY;
						}
						try {
							applyAxisMovement(axisState);
						}
						catch (std::exception& e) {
							LOG(INFO) << "Exception caught while applying virtual step movement: " << e.what();
						}
					}
					else if (gameType == 9999) {
						applyClickMovement();
					}
					else if (gameType == 3) {
						applyGripMovement();
					}
					if (gameType == 4 || gameType == 5) {
						applyKeyMovement();
					}
					inputStateChanged = false;
				}
			}
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

	int WalkInPlaceTabController::computeDMean(float sMean, arma::rowvec mM) {
		int idx = 0;
		float dM = 9999;
		for (int i = 0; i < mM.n_cols; i++) {
			float dMi = std::abs(sMean - mM(i));
			if (dMi < dM) {
				dM = dMi;
				idx = i;
			}
		}
		return idx;
	}

	void WalkInPlaceTabController::stopMovement() {
		try {
			if (validSample) {
				uint32_t deviceId = ovrwipCNTRLID;
				if (gameType == 0 || gameType == 1 || gameType == 2) {
					vr::VRControllerAxis_t axisState;
					axisState.x = 0;
					axisState.y = 0;
					try {
						vrwalkinplace::VRWalkInPlace vrwalkinplace;
						vrwalkinplace.connect();
						if (gameType != 1) {
							vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonUnpressed, deviceId, vr::k_EButton_SteamVR_Touchpad, 0.0);
						}
						vrwalkinplace.openvrAxisEvent(deviceId, vr::k_EButton_SteamVR_Touchpad, axisState);
						vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonUntouched, deviceId, vr::k_EButton_SteamVR_Touchpad, 0.0);
					}
					catch (std::exception& e) {
						//LOG(INFO) << "Exception caught while stopping virtual step movement: " << e.what();
					}
				}
				else if (gameType == 3) {
					try {
						vrwalkinplace::VRWalkInPlace vrwalkinplace;
						vrwalkinplace.connect();
						vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonUnpressed, deviceId, vr::k_EButton_Grip, 0.0);
					}
					catch (std::exception& e) {
						//LOG(INFO) << "Exception caught while applying virtual grip movement: " << e.what();
					}
				}
				else if (gameType == 9999) { //click only disabled atm
					try {
						vrwalkinplace::VRWalkInPlace vrwalkinplace;
						vrwalkinplace.connect();
						vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonUnpressed, deviceId, vr::k_EButton_SteamVR_Touchpad, 0.0);
					}
					catch (std::exception& e) {
						//LOG(INFO) << "Exception caught while stopping virtual teleport movement: " << e.what();
					}
				}
				else if (gameType == 4) {
#if defined _WIN64 || defined _LP64
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
				else if (gameType == 5) {
#if defined _WIN64 || defined _LP64
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
			}
			if (useTrackers && trkrSample.n_cols > 1) {
				trkrSample.shed_cols(0, trkrSample.n_cols - 1);
			}
			if (cntrlSample.n_cols > 1) {
				cntrlSample.shed_cols(0, cntrlSample.n_cols - 1);
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

	void WalkInPlaceTabController::applyVirtualStep() {
		uint32_t deviceId = ovrwipCNTRLID;
		vr::VRControllerAxis_t axisState;
		axisState.x = 0;
		axisState.y = 1;
		applyAxisMovement(axisState);
	}

	void WalkInPlaceTabController::applyAxisMovement(vr::VRControllerAxis_t axisState) {
		uint32_t deviceId = ovrwipCNTRLID;
		try {
			vrwalkinplace::VRWalkInPlace vrwalkinplace;
			vrwalkinplace.connect();
			if (gameType == 0 || gameType == 1 || gameType == 2) {
				vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonTouched, deviceId, vr::k_EButton_SteamVR_Touchpad, 0.0);
				vrwalkinplace.openvrAxisEvent(deviceId, vr::k_EButton_SteamVR_Touchpad, axisState);
				if (gameType != 1) {
					if (pressedFlag || gameType == 2) {
						vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonPressed, deviceId, vr::k_EButton_SteamVR_Touchpad, 0.0);
					}
					else {
						vrwalkinplace.openvrButtonEvent(vrwalkinplace::ButtonEventType::ButtonUnpressed, deviceId, vr::k_EButton_SteamVR_Touchpad, 0.0);
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
		unnTouchedCount = 0;
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
		unnTouchedCount = 0;
	}

	void WalkInPlaceTabController::applyKeyMovement() {
		if (gameType == 4) {
			if (pressedFlag) {
#if defined _WIN64 || defined _LP64
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
		else if (gameType == 5) {
			if (pressedFlag) {
#if defined _WIN64 || defined _LP64
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
