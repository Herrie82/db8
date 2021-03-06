// Copyright (c) 2009-2018 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0


#include "db/MojDbPermissionEngine.h"
#include "db/MojDb.h"
#include "db/MojDbKind.h"
#include "db/MojDbServiceDefs.h"
#include "core/MojObject.h"

const MojChar* const MojDbPermissionEngine::AllowKey = _T("allow");
const MojChar* const MojDbPermissionEngine::DenyKey = _T("deny");
const MojChar* const MojDbPermissionEngine::PermissionsKey = _T("permissions");
const MojChar* const MojDbPermissionEngine::PermissionsEnabledKey = _T("permissionsEnabled");
const MojChar* const MojDbPermissionEngine::WildcardOperation = _T("*");

//db.permissionEngine

MojDbPermissionEngine::MojDbPermissionEngine()
: MojDbPutHandler(MojDbKindEngine::PermissionId, PermissionsKey),
  m_enabled(true)
{
}

MojDbPermissionEngine::~MojDbPermissionEngine()
{
}

MojErr MojDbPermissionEngine::close()
{
    LOG_TRACE("Entering function %s", __FUNCTION__);

	MojErr err = MojErrNone;
	MojErr errClose = MojDbPutHandler::close();
	MojErrAccumulate(err, errClose);
	m_types.clear();

	return err;
}

MojErr MojDbPermissionEngine::put(MojObject& perm, MojDbReq& req, bool putObj)
{
    LOG_TRACE("Entering function %s", __FUNCTION__);
#ifdef LMDB_ENGINE_SUPPORT
	MojAssertWriteLocked(m_db->isDbAppLockingEnabled(), m_db->schemaLock());
#else
	MojAssertWriteLocked(m_db->schemaLock());
#endif
	// pull params out of object
	MojString type;
	MojErr err = perm.getRequired(MojDbServiceDefs::TypeKey, type);
	MojErrCheck(err);
	MojString object;
	err = perm.getRequired(MojDbServiceDefs::ObjectKey, object);
	MojErrCheck(err);
	MojString caller;
	err = perm.getRequired(MojDbServiceDefs::CallerKey, caller);
	MojErrCheck(err);
	MojObject operations;
	err = perm.getRequired(MojDbServiceDefs::OperationsKey, operations);
	MojErrCheck(err);

	// validate caller
	err = validateWildcard(caller, MojErrDbInvalidCaller);
	MojErrCheck(err);

	// check owner permissions on kinds
	if (type == MojDbKind::PermissionType) {
		err = m_db->kindEngine()->checkPermission(object, OpKindUpdate, req);
		MojErrCheck(err);
	}

	// create operation map
	OperationMap opMap;
	for (MojObject::ConstIterator i = operations.begin();
		 i != operations.end(); ++i) {
		MojString valStr;
		err = i.value().stringValue(valStr);
		MojErrCheck(err);
		Value val;
		err = valueFromString(valStr, val);
		MojErrCheck(err);
		err = opMap.put(i.key(), val);
		MojErrCheck(err);
	}
	// put object
	if (putObj) {
		MojString id;
		err = id.format(_T("_permissions/%s-%s-%s"), type.data(), object.data(), caller.data());
		MojErrCheck(err);
		err = perm.put(MojDb::IdKey, id);
		MojErrCheck(err);
		err = perm.putString(MojDb::KindKey, MojDbKindEngine::PermissionId);
		MojErrCheck(err);

		MojDbAdminGuard adminGuard(req);
		err = m_db->put(perm, MojDbFlagForce, req);
		MojErrCheck(err);
	}
	// get or create object map
	TypeMap::Iterator typeIter;
	err = m_types.find(type, typeIter);
	MojErrCheck(err);
	if (typeIter == m_types.end()) {
		err = m_types.put(type, ObjectMap());
		MojErrCheck(err);
		err = m_types.find(type, typeIter);
		MojErrCheck(err);
	}
	// get or create caller map
	ObjectMap::Iterator objIter;
	err = typeIter->find(object, objIter);
	MojErrCheck(err);
	if (objIter == typeIter->end()) {
		err = typeIter->put(object, CallerMap());
		MojErrCheck(err);
		err = typeIter->find(object, objIter);
		MojErrCheck(err);
	}
	// update caller map
	err = objIter->put(caller, opMap);
	MojErrCheck(err);

	return MojErrNone;
}

MojDbPermissionEngine::Value MojDbPermissionEngine::check(const MojChar* type,
		const MojChar* object, const MojChar* caller, const MojChar* op) const
{
    LOG_TRACE("Entering function %s", __FUNCTION__);
	MojAssert(type && object && caller && op);

	TypeMap::ConstIterator typeIter = m_types.find(type);
	if (typeIter == m_types.end())
		return ValueUndefined;

	ObjectMap::ConstIterator objIter = typeIter->find(object);
	if (objIter == typeIter->end())
		return ValueUndefined;

	CallerMap::ConstIterator callerIter = objIter->find(caller);
	if (callerIter == objIter->end()) {
		MojSize callerLen = MojStrLen(caller);
		for (callerIter = objIter->begin(); callerIter != objIter->end(); ++callerIter) {
			if (matchWildcard(callerIter.key(), caller, callerLen))
				break;
		}
		if (callerIter == objIter->end())
			return ValueUndefined;
	}

	OperationMap::ConstIterator opIter = callerIter->find(op);
	if (opIter == callerIter->end())
		return ValueUndefined;

	return opIter.value();
}

MojErr MojDbPermissionEngine::configure(const MojObject& conf, MojDbReq& req)
{
    LOG_TRACE("Entering function %s", __FUNCTION__);
#ifdef LMDB_ENGINE_SUPPORT
	MojAssertWriteLocked(m_db->isDbAppLockingEnabled(), m_db->schemaLock());
#else
	MojAssertWriteLocked(m_db->schemaLock());
#endif
	// enabled
	conf.get(PermissionsEnabledKey, m_enabled);
	// built-in permissions
	MojErr err = MojDbPutHandler::configure(conf, req);
	MojErrCheck(err);

	return MojErrNone;
}

MojErr MojDbPermissionEngine::valueFromString(const MojString& str, Value& valOut)
{
    LOG_TRACE("Entering function %s", __FUNCTION__);

	valOut = ValueUndefined;
	if (str == AllowKey) {
		valOut = ValueAllow;
	} else if (str == DenyKey) {
		valOut = ValueDeny;
	} else {
		MojErrThrowMsg(MojErrDbInvalidPermissions, _T("db: invalid permission value - '%s'"), str.data());
	}
	return MojErrNone;
}
