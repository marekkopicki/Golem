/** @file Data.h
 * 
 * 
 * @author	Marek Kopicki
 *
 * @copyright  Copyright (C) 2015 Marek Kopicki, University of Birmingham, UK
 *
 * @license  This file copy is licensed to you under the terms described in
 *           the License.txt file included in this distribution.
 *
 */

#pragma once
#ifndef _GOLEM_CTRL_DATA_H_
#define _GOLEM_CTRL_DATA_H_

//------------------------------------------------------------------------------

#include <Golem/Ctrl/Controller.h>
#include <Golem/Sys/XMLParser.h>
#include <Golem/Sys/XMLData.h>

//------------------------------------------------------------------------------

namespace golem {

//------------------------------------------------------------------------------

void XMLData(std::vector<Real> &seq, XMLContext* context, bool create = false);

//------------------------------------------------------------------------------

/** Reads/writes object from/to a given XML context */
void XMLData(GenCoord &val, XMLContext* context, bool create = false);
void XMLData(ExpCoord &val, XMLContext* context, bool create = false);
void XMLData(ConfigspaceCoord &val, const char* attr, XMLContext* context, bool create = false);

void XMLData(Joint::Desc &val, XMLContext* context, bool create = false);
void XMLData(Chain::Desc &val, XMLContext* context, bool create = false);
void XMLData(Controller::Desc &val, XMLContext* context, bool create = false);

//------------------------------------------------------------------------------

};	// namespace

#endif /*_GOLEM_CTRL_DATA_H_*/
