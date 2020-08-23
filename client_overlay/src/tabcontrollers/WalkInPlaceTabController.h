
#pragma once

#include <QObject>
#include <memory>
#include <openvr.h>
#include <vector>
#include <set>
#include <vrwalkinplace.h>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <mlpack/core.hpp>
#include <queue>

class QQuickWindow;

namespace fs = boost::filesystem;

// application namespace
namespace walkinplace {

	// forward declaration
	class OverlayController;

	enum InputType {
		touchpad = 0,
		joystick = 1,
		grip = 2,
		keyWASD = 3,
		keyArrow = 4
	};

	enum PaceControlType {
		AvgControllerModelMatch,
		AvgHMDModelMatch,
		AvgControllerHMDModelMatch,
		MinMaxControllerSpeed,
		CurControllerModelMatch,
		CurHMDModelMatch
	};

	enum TrackedDeviceLoc {
		TrackedDeviceOnHands = 0,
		TrackedDeviceOnFeet = 1
	};

	struct GameType {
		InputType inputType;
		bool useClick = true;
		bool alwaysHeld = false;
		bool useAxis = true;
	};

	struct greater
	{
		template<class T>
		bool operator()(T const &a, T const &b) const { return a > b; }
	};

	struct WalkInPlaceProfile {
		std::string profileName;
		std::string modelFile = "temp.csv";

		bool wipEnabled = false;
		bool trackHMDVel = true;
		bool trackHMDRot = true;
		int gameType = 0;
		int velControl = 0;
		int hmdType = 0;
		float minTouch = 0.35;
		float midTouch = 0.83;
		float maxTouch = 1.0;
		int cntrl1Type = vr::TrackedDeviceClass::TrackedDeviceClass_Controller;
		int cntrl2Type = vr::TrackedDeviceClass::TrackedDeviceClass_Controller;
		int trkr1Type = vr::TrackedDeviceClass::TrackedDeviceClass_GenericTracker;
		int trkr2Type = vr::TrackedDeviceClass::TrackedDeviceClass_GenericTracker;
		int cntrl1Idx = -1;
		int cntrl2Idx = -1;
		int trkr1Idx = -1;
		int trkr2Idx = -1;
	};

	struct DeviceInfo {
		std::string serial;
		vr::ETrackedDeviceClass deviceClass = vr::TrackedDeviceClass_Invalid;
		uint32_t openvrId = 0;
		uint32_t renderModelIndex = 0;
		vr::VROverlayHandle_t renderModelOverlay = vr::k_ulOverlayHandleInvalid;
		std::string renderModelOverlayName;
		vr::RenderModel_t * model;
		bool isTrackedAsCNTRL = false;
		bool isTrackedAsTRKR = false;
	};


	class WalkInPlaceTabController : public QObject {
		Q_OBJECT

	private:
		OverlayController * parent;
		QQuickWindow* widget;
		vrwalkinplace::VRWalkInPlace vrwalkinplace;

		std::vector<std::shared_ptr<DeviceInfo>> deviceInfos;
		std::set<uint32_t> deviceIdSet;
		uint32_t maxValidDeviceId = 0;

		std::thread identifyThread;

		std::vector<WalkInPlaceProfile> walkInPlaceProfiles;

		vr::TrackedDevicePose_t latestDevicePoses[vr::k_unMaxTrackedDeviceCount];

		vr::VRBoneTransform_t latestBoneTransforms[vr::k_unMaxTrackedDeviceCount];

		arma::mat dataModel;
		arma::mat hmdSample;
		arma::mat cntrlSample;
		arma::mat trkrSample;
		arma::rowvec hmdVels;
		arma::rowvec trkrVels;
		arma::rowvec modelCNTRL1;
		arma::rowvec modelCNTRL2;

		const int HMD_Y_VEL_IDX = 0;
		const int CNTRL1_Y_VEL_IDX = 1;
		const int CNTRL2_Y_VEL_IDX = 2;
		const int TRKR1_Y_VEL_IDX = 3;
		const int TRKR2_Y_VEL_IDX = 4;
		const int HMD_YAW_VEL_IDX = 5;
		const int HMD_PITCH_VEL_IDX = 6;
		const int HMD_X_VEL_IDX = 7;
		const int HMD_Z_VEL_IDX = 8;
		const int TOUCH_VAL_IDX = 9;

		int currentProfileIdx = -1;

		std::string model_file_type = ".csv";
		std::string model_file_name = "default";

		vr::HmdVector3d_t lastHmdPos = { 0, 0, 0 };
		vr::HmdVector3d_t lastHmdRot = { 0, 0, 0 };

		vr::VROverlayHandle_t overlayHandle;

		bool wipEnabled = false;
		int validSample = 0;
		bool hasTwoControllers = false;
		bool initializedProfile = false;
		bool initializedDriver = false;
		bool initializedDataModel = false;
		bool dataTrainingRequired = false;

		bool trackHMDVel = true;
		bool trackHMDRot = true;
		bool validTRKRSample = false;
		bool useContDirForStraf = false;
		bool useContDirForRev = false;
		bool pressedFlag = false;

		std::shared_ptr <GameType> gameType;
		PaceControlType paceControl = PaceControlType::AvgControllerModelMatch;
		int hmdType = 0;
		int lastValidHMDSampleMKi = 0;
		int lastCNTRLSampleMKi = 0;
		int lastValidTRKRSampleMKi = 0;
		int stopCallCount = 0;

		int maxSNHMD = 16;
		int startSNHMD = 12;
		int reqSNHMD = 4;
		int maxSNTRKR = 23;
		int startSNTRKR = 18;
		int reqSNTRKR = 12;
		int maxSNCNTRL = 12;
		int reqSNCNTRL = 10;

		uint64_t controller1ID = vr::k_unTrackedDeviceIndexInvalid;
		uint64_t controller2ID = vr::k_unTrackedDeviceIndexInvalid;
		uint64_t hmdID = vr::k_unTrackedDeviceIndexInvalid;
		uint64_t tracker1ID = vr::k_unTrackedDeviceIndexInvalid;
		uint64_t tracker2ID = vr::k_unTrackedDeviceIndexInvalid;
		uint64_t ovrwipCNTRLID = vr::k_unTrackedDeviceIndexInvalid;
		uint64_t directionDevice = vr::k_unTrackedDeviceIndexInvalid;
		std::queue<uint64_t> identifyDeviceQ;

		float hmdVelVariance = 0.07;
		float hmdMinDVPerSN = 0.75;
		float trkrVariance = 0.37;
		float cntrlVariance = 0.37;
		float hmdMaxPROTVel = 0;
		float hmdMaxYROTVel = 0;
		float hmdMaxXVel = 0;
		float hmdMaxZVel = 0;
		float minHMDPeakVal = 0.0;
		float maxCNTRLVal = 0.0;
		float avgCNTRLVal = 0.0;
		float minTRKRPeakVal = 0.0;
		float maxTRKRPeakVal = 0.0;
		float sNValidTouch = 0;
		float minTouch = 0.35;
		float midTouch = 0.7;
		float maxTouch = 1.0;
		float touchX = 0;
		float touchY = 0;
		float hmdYaw = 0;
		float contYaw = 0;
		float contRoll = 0;
		float contPitch = 0;

		const double timeStep = 1.0 / 40.0;
		const double dT = timeStep * 1000;
		const double identifyDeviceTO = 3000;
		double timeLastTick = 0.0;
		double timeLastVelTick = 0.0;
		double timeLastCNTRLSN = 0.0;
		double timeLastHaptic = 0.0;
		double minHapticTO = 6;
		double identifyDeviceLast = 0;
		double timeLastGraphPoint = 0.0;

		std::pair<float, int> computeSNDelta(arma::mat sN, arma::mat mN);
		std::pair<float, int> computeSNDeltaOffset(arma::mat sN, arma::mat mN);
		std::pair<int, int> computeSNDV(arma::mat sN, arma::mat mN);
		int findMNIdxGTS(float s, arma::mat mN);
		float findMinPeakMN(int startERR, arma::rowvec mN, float lowRange);

		void loadDataModel();
		void clearSamplesAndModel();

		void identifyDevice(unsigned deviceIndex, unsigned short duration);

		void runSampleOnModel();
		void testHMDSample(double dt);
		void testTRKRSample(double dt);
		float determineInputScale();
		void determineCurDirection();

		void stopMovement();
		void clearClickedFlag();
		void stopClickMovement();
		void applyAxisMovement(vr::VRControllerAxis_t axisState);
		void applyClickMovement();
		void applyGripMovement();
		void applyKeyMovement();

	public:
		~WalkInPlaceTabController();
		void initStage1();
		void initStage2(OverlayController* parent, QQuickWindow* widget);

		void eventLoopTick();
		void handleEvent(const vr::VREvent_t& vrEvent);

		Q_INVOKABLE unsigned getDeviceCount();
		Q_INVOKABLE QString getDeviceSerial(unsigned index);
		Q_INVOKABLE unsigned getDeviceId(unsigned index);
		Q_INVOKABLE int getDeviceClass(unsigned index);
		Q_INVOKABLE double getStepTime();
		Q_INVOKABLE int getGameType();
		Q_INVOKABLE int getHMDType();
		Q_INVOKABLE int getPaceControl();
		Q_INVOKABLE bool getDisableHMD();
		Q_INVOKABLE bool getTrackHMDRot();
		Q_INVOKABLE float getMinTouch();
		Q_INVOKABLE float getMidTouch();
		Q_INVOKABLE float getMaxTouch();
		Q_INVOKABLE bool getDeviceEnabled(int devClass, int devIdx, int mode);
		Q_INVOKABLE bool isWIPEnabled();
		Q_INVOKABLE bool isStepDetected();
		Q_INVOKABLE QList<qreal> getGraphPoses(double tdiff);
		Q_INVOKABLE QList<qreal> getHMDSample();
		Q_INVOKABLE QList<qreal> getCNTRLSample();
		Q_INVOKABLE QList<qreal> getTRKRSample();
		Q_INVOKABLE void applyVirtualStep(float y);
		Q_INVOKABLE QList<qreal> getModelData();
		Q_INVOKABLE QList<qreal> trainingDataSample(float scaleSpeed, double tdiff);
		Q_INVOKABLE void completeTraining();

		Q_INVOKABLE unsigned getProfileCount();
		Q_INVOKABLE QString getProfileName(unsigned index);

		Q_INVOKABLE QList<QString> getDataModelNames();

	public slots:
		void enableWIP(bool enable);
		void setTrackHMDVel(bool val);
		void setTrackHMDRot(bool val);
		void setDirectionDevice(int choice);
		void enableDevice(int deviceClass, int devIdx, bool enable, int mode);
		void setHMDMaxVari(float val);
		void setMinTouch(float value);
		void setMidTouch(float value);
		void setMaxTouch(float value);
		void setGameType(int gameType);
		void setHMDType(int gameType);
		void setPaceControl(int paceControl);

		void addProfile(QString name);
		void applyProfile(unsigned index);
		void deleteProfile(unsigned index);

		void reloadSettings();
		void reloadProfiles();
		void saveSettings();
		void saveProfiles();

		void addDataModel(QString name);
		void applyDataModel(QString name);
		void deleteDataModel(QString name);

	};

} // namespace walkinplace
