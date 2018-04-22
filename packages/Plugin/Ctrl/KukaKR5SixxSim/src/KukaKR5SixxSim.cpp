/** @file KukaKR5SixxSim.cpp
 * 
 * @author	Marek Kopicki
 *
 * @copyright  Copyright (C) 2015 Marek Kopicki, University of Birmingham, UK
 *
 * @license  This file copy is licensed to you under the terms described in
 *           the License.txt file included in this distribution.
 *
 */

#include <Golem/Ctrl/KukaKR5SixxSim/KukaKR5SixxSim.h>
#include <Golem/Ctrl/KukaKR5SixxSim/Data.h>
#include <Golem/Sys/LoadObjectDesc.h>

//------------------------------------------------------------------------------

using namespace golem;

//------------------------------------------------------------------------------

GOLEM_LIBRARY_DECLDIR void loadControllerDesc(void* pContext, void* pXMLContext, void* pControllerDesc) {
	loadObjectDesc<golem::KukaKR5SixxSim::Desc, Controller::Desc::Ptr>((Context*)pContext, (XMLContext*)pXMLContext, (Controller::Desc::Ptr*)pControllerDesc);
}

//------------------------------------------------------------------------------

KukaKR5SixxSim::KukaKR5SixxSim(golem::Context& context) : KukaKR5Sixx(context) {
}

KukaKR5SixxSim::~KukaKR5SixxSim() {
	SingleCtrl::release();
}

//------------------------------------------------------------------------------
