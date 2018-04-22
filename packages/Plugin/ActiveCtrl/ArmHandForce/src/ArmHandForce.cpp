/** @file ArmHandForce.cpp
 * 
 * @author	Marek Kopicki
 *
 * @copyright  Copyright (C) 2015 Marek Kopicki, University of Birmingham, UK
 *
 * @license  This file copy is licensed to you under the terms described in
 *           the License.txt file included in this distribution.
 *
 */

#include <Golem/ActiveCtrl/ArmHandForce/ArmHandForce.h>
#include <Golem/Sys/XMLData.h>
#include <Golem/Math/Data.h>
#include <GL/glut.h>
#include <iomanip>

//#define _GOLEM_HITHANDII_DEBUG
#ifdef _GOLEM_HITHANDII_DEBUG
#include <Golem/Ctrl/DLR/DLRHitHandII.h>
#endif // _GOLEM_HITHANDII_DEBUG

using namespace golem;

//------------------------------------------------------------------------------

GOLEM_LIBRARY_DECLDIR void* golemDescLoader() {
	// Create description
	return new ArmHandForce::Desc();
}

//------------------------------------------------------------------------------

void ArmHandForce::Desc::load(golem::Context& context, const golem::XMLContext* xmlcontext) {
	ActiveCtrl::Desc::load(context, xmlcontext);

	golem::XMLData(dynamic_cast<ActiveCtrlFT::Desc&>(*armCtrlDesc), xmlcontext->getContextFirst("ctrl_arm"), false);
	golem::XMLData(dynamic_cast<ActiveCtrlForce::Desc&>(*handCtrlDesc), xmlcontext->getContextFirst("ctrl_hand"), false);

	golem::XMLData(ftGain, xmlcontext->getContextFirst("ft_sensor gain"), false);
	golem::XMLData(ftLimit, xmlcontext->getContextFirst("ft_sensor limit"), false);
	golem::XMLDataSeq(fLimit, "c", xmlcontext->getContextFirst("ft_sensor limit"), false, golem::REAL_ZERO);

	golem::XMLDataSeq(impedStiffMin, "c", xmlcontext->getContextFirst("impedance stiff_min"), false, golem::REAL_ZERO);
	golem::XMLDataSeq(impedStiffMax, "c", xmlcontext->getContextFirst("impedance stiff_max"), false, golem::REAL_ZERO);
	golem::XMLData("stiff_steps", impedStiffSteps, xmlcontext->getContextFirst("impedance"), false);
	golem::XMLData("stiff_step_init", impedStiffStepInit, xmlcontext->getContextFirst("impedance"), false);

	golem::XMLDataSeq(impedDampMin, "c", xmlcontext->getContextFirst("impedance damp_min"), false, golem::REAL_ZERO);
	golem::XMLDataSeq(impedDampMax, "c", xmlcontext->getContextFirst("impedance damp_max"), false, golem::REAL_ZERO);
	golem::XMLData("damp_steps", impedDampSteps, xmlcontext->getContextFirst("impedance"), false);
	golem::XMLData("damp_step_init", impedDampStepInit, xmlcontext->getContextFirst("impedance"), false);

	golem::XMLData("show_force", appearanceShowForce, xmlcontext->getContextFirst("appearance"), false);
	golem::XMLData("show_force_torque", appearanceShowForceTorque, xmlcontext->getContextFirst("appearance"), false);
	golem::XMLData("f_scale", appearanceFScale, xmlcontext->getContextFirst("appearance"), false);
	golem::XMLData(appearanceFTSize, xmlcontext->getContextFirst("appearance ft_size"), false);
}

//------------------------------------------------------------------------------

ArmHandForce::ArmHandForce(golem::Planner &planner, const Sensor::Seq& sensorSeq) : ActiveCtrl(planner, sensorSeq), arm(nullptr), hand(nullptr), ftSensor(nullptr), key(0) {
}
	
ArmHandForce::~ArmHandForce() {
	// unregister before releasing, required because they use each other resources
	if (armCtrl != nullptr) (void)armCtrl->registerIO();
	if (handCtrl != nullptr) (void)handCtrl->registerIO();
	// automatic release
}

void ArmHandForce::create(const Desc& desc) {
	ActiveCtrl::create(desc); // throws

	// controllers
	if (controllerIDSeq.size() < 2)
		throw Message(Message::LEVEL_CRIT, "ArmHandForce::create(): At least two controllers required!");
	arm = is<SingleCtrl>(controllerIDSeq[0].findController(const_cast<golem::Controller&>(controller)));
	hand = is<SingleCtrl>(controllerIDSeq[1].findController(const_cast<golem::Controller&>(controller)));
	if (!arm || !hand)
		throw Message(Message::LEVEL_CRIT, "ArmHandForce::create(): SingleCtrl controllers required!");
	armInfo = arm->getStateInfo();
	handInfo = hand->getStateInfo();

	// sensors
	ftSensor = nullptr;
	if (sensorSeq.size() > 0) {
		ftSensor = is<FT>(sensorSeq[0]);
		if (!ftSensor)
			context.warning("ArmHandForce::create(): unknown sensor\n");
	}
	if (!ftSensor)
		context.warning("ArmHandForce::create(): F/T sensor not available\n");
	else {
		context.info("ArmHandForce::create(): using sensor %s\n", ftSensor->getID().c_str());
	}

	// emergency mode
	emgMode = true;

	ftFrame = ftSensor ? ftSensor->getCurrentCalibration()->getLocalFrame() : Mat34::identity();
	ftFrameInv.setInverse(ftFrame);
	ftGain = desc.ftGain;
	ftLimit = desc.ftLimit;
	fLimit = desc.fLimit;

	// TODO check size of all chains
	if (std::min(desc.impedStiffMin.size(), desc.impedDampMin.size()) < (size_t)handInfo.getJoints(handInfo.getChains().begin()).size())
		throw Message(Message::LEVEL_CRIT, "ArmHandForce::create(): Invalid number of impedance control parameters");
	impedStiffMin = desc.impedStiffMin;
	impedStiffMax = desc.impedStiffMax;
	impedStiffSteps = desc.impedStiffSteps;
	impedStiffStep = desc.impedStiffStepInit;
	impedStiff = impedStiffMin;
	impedDampMin = desc.impedDampMin;
	impedDampMax = desc.impedDampMax;
	impedDampSteps = desc.impedDampSteps;
	impedDampStep = desc.impedDampStepInit;
	impedDamp = impedDampMin;

	// update Hand impedance parameters
	level("stiffness", impedStiffMin, impedStiffMax, impedStiffSteps, impedStiffStep, impedStiff);
	level("damping", impedDampMin, impedDampMax, impedDampSteps, impedDampStep, impedDamp);

	simMode = false;
	simModeVec.setZero();
	simModeScr.setZero();

	simForceGain = desc.simForceGain;
	simArmForceMode = FORCE_MODE_DISABLED;
	simHandForceMode = FORCE_MODE_DISABLED;

	appearanceShowForce = desc.appearanceShowForce;
	appearanceShowForceTorque = desc.appearanceShowForceTorque;
	appearanceFScale = desc.appearanceFScale;
	appearanceFTSize = desc.appearanceFTSize;

	emergencyModeHandler = desc.emergencyModeHandler;

	armForceReader = desc.armForceReader;
	armForceReaderDflt = [=] (const golem::Controller::State&, RealSeq& force) {
		if (emgMode) {
			const Twist& wrench = *reinterpret_cast<const Twist*>(force.data());
			for (size_t i = 0; i < 3; ++i)
				if (Math::abs(wrench.v[i]) > ftLimit.v[i] || Math::abs(wrench.w[i]) > ftLimit.w[i])
					throw Message(Message::LEVEL_NOTICE, "ArmHandForce::armForceReaderDflt(): FT = {(%3.3lf, %3.3lf, %3.3lf), (%3.3lf, %3.3lf, %3.3lf)}", wrench.v.x, wrench.v.y, wrench.v.z, wrench.w.x, wrench.w.y, wrench.w.z);
		}
	};

	armForce.setZero();
	desc.armCtrlDesc->forceReader = [=] (const golem::Controller::State& state, RealSeq& force) {
		force.assign(6, REAL_ZERO);
		Twist& wrench = *reinterpret_cast<Twist*>(force.data());

		// use simulated force
		if (simArmForceMode != FORCE_MODE_DISABLED) {
			if (simArmForceMode == FORCE_MODE_TORQUE)
				wrench.w.arrayMultiply(simModeVec, simForceGain.w);
			if (simArmForceMode == FORCE_MODE_FORCE)
				wrench.v.arrayMultiply(simModeVec, simForceGain.v);
		}
		// read from F/T sensor
		else {
			if (ftSensor) {
				FT::Data data;
				ftSensor->read(data);
				wrench = data.wrench;
			}
		}

		bool emergency = false;
		try {
			golem::CriticalSectionWrapper csw(csData);
			// use customised force reader if available, otherwise the default one
			armForceReader ? armForceReader(state, force) : armForceReaderDflt(state, force);
		}
		catch (const std::exception& ex) {
			// if F/T threshold limit is reached
			if (armCtrl->getMode() != ActiveCtrlForce::MODE_EMERGENCY) {
				emergency = true;
				// print exception
				context.write("%s\n", ex.what());
				// set emergency mode
				armCtrl->setMode(ActiveCtrlForce::MODE_EMERGENCY);
				handCtrl->setMode(ActiveCtrlForce::MODE_EMERGENCY);
				// stop controller and cleanup the command queue
				controller.stop();
			}
		}

		{
			golem::CriticalSectionWrapper csw(csData);
			armForce = reinterpret_cast<const golem::Twist&>(*force.data());
			// emergency mode handler
			if (emergency && emergencyModeHandler) emergencyModeHandlerThread.start(emergencyModeHandler);
		}
		
		// multiply by gain
		if (simArmForceMode == FORCE_MODE_DISABLED && ftSensor) wrench.arrayMultiply(wrench, ftGain);
		// transform to the tool frame
		wrench.adjointTransposedTransform(wrench, ftFrameInv);
	};
	armCtrl = desc.armCtrlDesc->create(*arm); // create and install callback (throws)
	
	offsetIdx = 0;
	handForceReader = desc.handForceReader;
	handForceReaderDflt = [=] (const golem::Controller::State&, RealSeq& force) {
		if (emgMode) {
			const size_t size = std::min(force.size(), fLimit.size());
			for (size_t i = 0; i < size; ++i)
				if (Math::abs(force[i]) > fLimit[i])
					throw Message(Message::LEVEL_NOTICE, "ArmHandForce::handForceReaderDflt(): F[%u/%u] = (%3.3lf)", i + 1, size, force[i]);
		}
	};

	desc.handCtrlDesc->forceReader = [=] (const golem::Controller::State& state, RealSeq& force) {
		// no force by default
		force.assign(state.getInfo().getJoints().size(), REAL_ZERO);

		// use simulated force
		if (simHandForceMode != FORCE_MODE_DISABLED) {
			const Vec3 simForce(simModeVec.z*simForceGain.v.z, simModeVec.x*simForceGain.v.x, simModeVec.y*simForceGain.v.y);

			Chainspace::Index i = state.getInfo().getChains().begin() + simHandForceMode - 1; // simHandForceMode corresponds to chain
			for (Configspace::Index j = state.getInfo().getJoints(i).begin(); j < state.getInfo().getJoints(i).end(); ++j) {
				const size_t k = j - state.getInfo().getJoints().begin(); // from the first joint
				const size_t l = j - state.getInfo().getJoints(i).begin(); // from the first joint in the chain
				force[k] = simForce[std::min(size_t(2), l)];
			}
		}
		// read from the state variable (if supported)
		else {
			const ptrdiff_t forceOffset = hand->getReservedOffset(Controller::RESERVED_INDEX_FORCE_TORQUE);
			if (forceOffset != Controller::ReservedOffset::UNAVAILABLE) {
				for (Configspace::Index j = state.getInfo().getJoints().begin(); j < state.getInfo().getJoints().end(); ++j) {
					const size_t k = j - state.getInfo().getJoints().begin();
					force[k] = state.get<ConfigspaceCoord>(forceOffset)[j];
				}
			}
		}

		bool emergency = false;
		try {
			golem::CriticalSectionWrapper csw(csData);
			// use customised force reader if available, otherwise the default one
			handForceReader ? handForceReader(state, force) : handForceReaderDflt(state, force);
		}
		catch (const std::exception& ex) {
			// if force threshold limit is reached
			if (handCtrl->getMode() != ActiveCtrlForce::MODE_EMERGENCY) {
				emergency = true;
				// print exception
				context.write("%s\n", ex.what());
				// set emergency mode
				armCtrl->setMode(ActiveCtrlForce::MODE_EMERGENCY);
				handCtrl->setMode(ActiveCtrlForce::MODE_EMERGENCY);
				// stop controller and cleanup the command queue
				controller.stop();
			}
		}

		{
			golem::CriticalSectionWrapper csw(csData);
			handForce = force;
			// emergency mode handler
			if (emergency && emergencyModeHandler) emergencyModeHandlerThread.start(emergencyModeHandler);
		}
	};
	desc.handCtrlDesc->actionFilter = [=] (const golem::Controller::State& prev, golem::Controller::State& next, bool bSendPrev, bool bSendNext) {
		// only if there are no awaiting waypoints
		if (!bSendNext) {
			golem::CriticalSectionWrapper csw(csData);

			const ptrdiff_t stiffnessOffset = hand->getReservedOffset(Controller::RESERVED_INDEX_STIFFNESS);
			const ptrdiff_t dampingOffset = hand->getReservedOffset(Controller::RESERVED_INDEX_DAMPING);

			for (Chainspace::Index i = next.getInfo().getChains().begin(); i < next.getInfo().getChains().end(); ++i)
				for (Configspace::Index j = next.getInfo().getJoints(i).begin(); j < next.getInfo().getJoints(i).end(); ++j) {
					const size_t k = j - next.getInfo().getJoints(i).begin(); // from the first joint in current chain
					if (stiffnessOffset != Controller::ReservedOffset::UNAVAILABLE)
						next.get<ConfigspaceCoord>(stiffnessOffset)[j] = impedStiff[k];
					if (dampingOffset != Controller::ReservedOffset::UNAVAILABLE)
						next.get<ConfigspaceCoord>(dampingOffset)[j] = impedDamp[k];
				}
		}
	};
	handCtrl = desc.handCtrlDesc->create(*hand); // create and install callback (throws)

	setActive(desc.active);
}

//------------------------------------------------------------------------------

void ArmHandForce::getLimits(golem::Twist &wrench) const {
	golem::CriticalSectionWrapper csw(csData);
	wrench = ftLimit;
}

void ArmHandForce::getLimits(RealSeq &force) const {
	golem::CriticalSectionWrapper csw(csData);
	force = fLimit;
}

void ArmHandForce::setLimits(const golem::Twist &wrench) {
	golem::CriticalSectionWrapper csw(csData);
	ftLimit = wrench;
}

void ArmHandForce::setLimits(const RealSeq &force) {
	golem::CriticalSectionWrapper csw(csData);
	fLimit = force;
}

void ArmHandForce::getArmForce(golem::Twist &wrench) const {
	golem::CriticalSectionWrapper csw(csData);
	wrench = armForce;
}

void ArmHandForce::getHandForce(RealSeq& force) const {
	golem::CriticalSectionWrapper csw(csData);
	force = handForce;
}

void ArmHandForce::setEmergencyModeHandler(ThreadTask::Function emergencyModeHandler) {
	golem::CriticalSectionWrapper csw(csData);
	this->emergencyModeHandler = emergencyModeHandler;
}

void ArmHandForce::setArmForceReaderr(ActiveCtrlForce::ForceReader armForceReader) {
	golem::CriticalSectionWrapper csw(csData);
	this->armForceReader = armForceReader;
}

void ArmHandForce::setHandForceReader(ActiveCtrlForce::ForceReader handForceReader) {
	golem::CriticalSectionWrapper csw(csData);
	this->handForceReader = handForceReader;
}

//------------------------------------------------------------------------------

void ArmHandForce::render() const {
	const bool isActiveArm = armCtrl->getMode() != ActiveCtrlForce::MODE_DISABLED;
	const bool isActiveHand = handCtrl->getMode() != ActiveCtrlForce::MODE_DISABLED;

	// current state
	golem::Controller::State state = controller.createState();
	controller.lookupState(context.getTimer().elapsed(), state);
	// workspace coords of each joint
	golem::WorkspaceJointCoord wjc;
	controller.jointForwardTransform(state.cpos, wjc);
	
	// display forces/torques
	renderer.reset();
	if (appearanceShowForce) {
		for (Configspace::Index i = info.getJoints().begin(); i < info.getJoints().end(); ++i) {
			const Mat34 frame = wjc[i];
			Real torque = REAL_ZERO;

			if (isActiveArm && armInfo.getJoints().contains(i)) {
				const idx_t j = i - armInfo.getJoints().begin();
				armCtrl->getForceOut(armJointTorques);
				torque = armJointTorques[j];
			}
			if (isActiveHand && handInfo.getJoints().contains(i)) {
				const idx_t j = i - handInfo.getJoints().begin();
				handCtrl->getForceOut(handJointTorques);
				torque = handJointTorques[j];
			}

			if (torque != REAL_ZERO) {
				Vec3 anchor, axis;
				controller.getJoints()[i]->getTrn().twist.toScrew(anchor, axis);
				axis.setMagnitude(appearanceFScale*torque);
				frame.R.multiply(axis, axis);
				const Vec3 p[2] = { frame.p, frame.p + axis };
				renderer.addLine(p[0], p[1], RGBA::MAGENTA);
			}
		}
	}
	if (appearanceShowForceTorque && ftSensor) {
		const Mat34 frame = ftSensor->getFrame();

		golem::Twist wrench;
		getArmForce(wrench);
		golem::Twist size(appearanceFTSize * (wrench.v / ftLimit.v), appearanceFTSize * (wrench.w / ftLimit.w));
		golem::Vec3 size3(Math::abs(size.v.x) > Math::abs(size.w.x) ? size.v.x : size.w.x, Math::abs(size.v.y) > Math::abs(size.w.y) ? size.v.y : size.w.y, Math::abs(size.v.z) > Math::abs(size.w.z) ? size.v.z : size.w.z);
		
		renderer.addAxis3D(frame.p, frame.p + frame.R * Vec3::axisX() * size3.x, RGBA(RGBA::RED._rgba.r, RGBA::RED._rgba.g, RGBA::RED._rgba.b, 127));
		renderer.addAxis3D(frame.p, frame.p + frame.R * Vec3::axisY() * size3.y, RGBA(RGBA::GREEN._rgba.r, RGBA::GREEN._rgba.g, RGBA::GREEN._rgba.b, 127));
		renderer.addAxis3D(frame.p, frame.p + frame.R * Vec3::axisZ() * size3.z, RGBA(RGBA::BLUE._rgba.r, RGBA::BLUE._rgba.g, RGBA::BLUE._rgba.b, 127));
	}

	renderer.render();
}
	
void ArmHandForce::keyboardHandler(int key, int x, int y) {
	this->key = key;

	switch (key) {
	case 32: // <space>
		if (armCtrl->getMode() != ActiveCtrlForce::MODE_DISABLED) armCtrl.get()->setMode(ActiveCtrlForce::MODE_DISABLED);
		if (handCtrl->getMode() != ActiveCtrlForce::MODE_DISABLED) handCtrl.get()->setMode(ActiveCtrlForce::MODE_DISABLED);
		simHandForceMode = FORCE_MODE_DISABLED;
		simArmForceMode = FORCE_MODE_DISABLED;
		break;
	case (GLUT_KEY_F1 | UIKeyboardMouseCallback::KEY_SPECIAL) : // F1
		armCtrl->setMode(ActiveCtrlForce::MODE_ENABLED);
		simHandForceMode = FORCE_MODE_DISABLED;
		simArmForceMode = FORCE_MODE_DISABLED;
		break;
	case (GLUT_KEY_F2 | UIKeyboardMouseCallback::KEY_SPECIAL) : // F2
		armCtrl->setMode(ActiveCtrlForce::MODE_ENABLED);
		simModeVec.setZero();
		simArmForceMode = simArmForceMode >= FORCE_MODE_FORCE ? FORCE_MODE_TORQUE : simArmForceMode + 1;
		simHandForceMode = FORCE_MODE_DISABLED;
		context.write("%s: simulated %s ON\n", arm->getName().c_str(), simArmForceMode == FORCE_MODE_FORCE ? "force" : "torque");
		return;
	case (GLUT_KEY_F3 | UIKeyboardMouseCallback::KEY_SPECIAL) : // F3
		handCtrl->setMode(ActiveCtrlForce::MODE_ENABLED);
		simHandForceMode = FORCE_MODE_DISABLED;
		simArmForceMode = FORCE_MODE_DISABLED;
		break;
	case (GLUT_KEY_F4 | UIKeyboardMouseCallback::KEY_SPECIAL) : // F4
		handCtrl->setMode(ActiveCtrlForce::MODE_ENABLED);
		simModeVec.setZero();
		simHandForceMode = simHandForceMode >= (U32)handInfo.getChains().size() ? 1 : simHandForceMode + 1;
		simArmForceMode = FORCE_MODE_DISABLED;
		context.write("%s: simulated %s ON\n", hand->getName().c_str(), hand->getChains()[handInfo.getChains().begin() + simHandForceMode - 1]->getName().c_str());
		return;
	case (GLUT_KEY_F12 | UIKeyboardMouseCallback::KEY_SPECIAL) : // F4
		emgMode = !emgMode;
		context.write("%s: Force limit emergency %s\n", arm->getName().c_str(), emgMode ? "ON" : "OFF");
		return;
#ifdef _GOLEM_HITHANDII_DEBUG
	case '-':
		offsetIdx = offsetIdx <= 0 ? (U32)handInfo.getJoints().size() - 1 : offsetIdx - 1;
		context.write("Offset joint: %d\n", offsetIdx + 1);
		return;
	case '+':
		offsetIdx = offsetIdx >= (U32)handInfo.getJoints().size() - 1 ? 0 : offsetIdx + 1;
		context.write("Offset joint: %d\n", offsetIdx + 1);
		return;
	case ',':
	{
		DLRHitHandII* pDLRHitHandII = dynamic_cast<DLRHitHandII*>(hand);
		if (pDLRHitHandII) {
			Real o[golem::DLRHitHandII::NUM_JOINTS];
			pDLRHitHandII->getOffset(o);
			o[offsetIdx] -= 0.005*REAL_PI;
			pDLRHitHandII->setOffset(o);
			context.write("<offset c1=\"%f\" c2=\"%f\" c3=\"%f\" c4=\"%f\" c5=\"%f\" c6=\"%f\" c7=\"%f\" c8=\"%f\" c9=\"%f\" c10=\"%f\" c11=\"%f\" c12=\"%f\" c13=\"%f\" c14=\"%f\" c15=\"%f\" c16=\"%f\" c17=\"%f\" c18=\"%f\" c19=\"%f\" c20=\"%f\"/>\n", o[0], o[1], o[2], o[3], o[4], o[5], o[6], o[7], o[8], o[9], o[10], o[11], o[12], o[13], o[14], o[15], o[16], o[17], o[18], o[19]);
		}
		return;
	}
	case '.':
	{
		DLRHitHandII* pDLRHitHandII = dynamic_cast<DLRHitHandII*>(hand);
		if (pDLRHitHandII) {
			Real o[golem::DLRHitHandII::NUM_JOINTS];
			pDLRHitHandII->getOffset(o);
			o[offsetIdx] += 0.005*REAL_PI;
			pDLRHitHandII->setOffset(o);
			context.write("<offset c1=\"%f\" c2=\"%f\" c3=\"%f\" c4=\"%f\" c5=\"%f\" c6=\"%f\" c7=\"%f\" c8=\"%f\" c9=\"%f\" c10=\"%f\" c11=\"%f\" c12=\"%f\" c13=\"%f\" c14=\"%f\" c15=\"%f\" c16=\"%f\" c17=\"%f\" c18=\"%f\" c19=\"%f\" c20=\"%f\"/>\n", o[0], o[1], o[2], o[3], o[4], o[5], o[6], o[7], o[8], o[9], o[10], o[11], o[12], o[13], o[14], o[15], o[16], o[17], o[18], o[19]);
		}
		return;
	}
#endif // _GOLEM_HITHANDII_DEBUG
	}
};

void ArmHandForce::mouseHandler(int button, int state, int x, int y) {
	if (state == GLUT_DOWN && button == GLUT_RIGHT_BUTTON) {
		simMode = true;
		simModeScr.set(x, y);
		simModeVec.setZero();
	}
	else if (state == GLUT_UP && button != 7 && button != 8) {
		simMode = false;
		simModeVec.setZero();
	}

	if (simMode) {
		simModeVec.z += button == 7 ? +Real(2.0) : button == 8 ? -Real(2.0) : Real(0.0);
	}

	if (simHandForceMode != FORCE_MODE_DISABLED && key == (GLUT_KEY_F5 | UIKeyboardMouseCallback::KEY_SPECIAL)) {
		if (button == 3 && impedStiffStep > 0) --impedStiffStep;
		if (button == 4 && impedStiffStep < impedStiffSteps) ++impedStiffStep;
		level("stiffness", impedStiffMin, impedStiffMax, impedStiffSteps, impedStiffStep, impedStiff);
		key = 0;
	}
	if (simHandForceMode != FORCE_MODE_DISABLED && key == (GLUT_KEY_F6 | UIKeyboardMouseCallback::KEY_SPECIAL)) {
		if (button == 3 && impedDampStep > 0) --impedDampStep;
		if (button == 4 && impedDampStep < impedDampSteps) ++impedDampStep;
		level("damping", impedDampMin, impedDampMax, impedDampSteps, impedDampStep, impedDamp);
		key = 0;
	}
}

void ArmHandForce::motionHandler(int x, int y) {
	if (simMode) {
		simModeVec.x = Real(simModeScr.x - x);
		simModeVec.y = Real(y - simModeScr.y); // (0, 0) at the upper left corner!
	}
}

//------------------------------------------------------------------------------

void ArmHandForce::level(const char* name, const RealSeq& min, const RealSeq& max, golem::U32 steps, golem::U32 step, RealSeq& val) {
	std::stringstream str;
	str << std::setprecision(2) << std::fixed;
	{
		golem::CriticalSectionWrapper csw(csData);
		for (size_t i = 0; i < val.size(); ++i) {
			val[i] = step == 0 ? REAL_ZERO : min[i]*Math::pow(max[i]/min[i], (step - REAL_ONE)/(steps - REAL_ONE));
			str << val[i] << (i < val.size() - 1 ? ", " : "");
		}
	}
	context.verbose("ArmHandForce::level(): %s: %s = (%s)\n", hand->getName().c_str(), name, str.str().c_str());
}

//------------------------------------------------------------------------------

void ArmHandForce::setActive(bool active) {
	armCtrl->setActive(active);
	handCtrl->setActive(active);
	Active::setActive(active);
}

//------------------------------------------------------------------------------
