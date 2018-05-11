////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#include "MaintenanceWorker.h"

#include "lib/Logger/Logger.h"
#include "Cluster/MaintenanceFeature.h"

namespace arangodb {

namespace maintenance {

MaintenanceWorker::MaintenanceWorker(arangodb::MaintenanceFeature & feature)
  : Thread("MaintenanceWorker"),
    _feature(feature), _curAction(nullptr), _loopState(eFIND_ACTION),
    _directAction(false) {

  return;

} // MaintenanceWorker::MaintenanceWorker


MaintenanceWorker::MaintenanceWorker(arangodb::MaintenanceFeature & feature,
  std::shared_ptr<Action> & directAction)
  : Thread("MaintenanceWorker"),
    _feature(feature), _curAction(directAction), _loopState(eRUN_FIRST),
    _directAction(true) {

  return;

} // MaintenanceWorker::MaintenanceWorker


void MaintenanceWorker::run() {
  bool more(false);

  while(eSTOP != _loopState && !_feature.isShuttingDown()){

    switch(_loopState) {
      case eFIND_ACTION:
        _curAction = _feature.findReadyAction();
        more = _curAction != nullptr;
        break;

      case eRUN_FIRST:
        more = _curAction->first().is(TRI_ERROR_ACTION_UNFINISHED);
        break;

      case eRUN_NEXT:
        more = _curAction->next().is(TRI_ERROR_ACTION_UNFINISHED);
        break;

      default:
        _loopState = eSTOP;
        LOG_TOPIC(ERR, Logger::CLUSTER)
          << "MaintenanceWorkerRun:  unexpected state (" << _loopState << ")";

    } // switch

    // determine next loop state
    nextState(more);

  } // while

} // MaintenanceWorker::run


void MaintenanceWorker::nextState(bool actionMore) {

  // bad result code forces actionMore to false
  if (_curAction && (!_curAction->result().ok()
                     || maintenance::FAILED == _curAction->getState()))
  {
    actionMore = false;
  } // if

  // actionMore means iterate again
  if (actionMore) {
    // There should be an valid _curAction
    if (_curAction) {
      if (eFIND_ACTION == _loopState) {
        _curAction->startStats();
        _loopState = eRUN_FIRST;
      } else {
        _curAction->incStats();
        _loopState = eRUN_NEXT;
      } // if

      // move execution to PreAction if it exists
      if (_curAction->getPreAction()) {
        std::shared_ptr<Action> tempPtr;

        _curAction->setState(maintenance::WAITING);
        tempPtr=_curAction;
        _curAction=_curAction->getPreAction();
        _curAction->setPostAction(tempPtr);
        _loopState = eRUN_FIRST;
      } // if
    } else {
      // this state should not exist, but deal with it
      _loopState = (_directAction ? eSTOP : eFIND_ACTION);
    } // else
  } else {
    // finish the current action
    if (_curAction) {
      _lastResult = _curAction->result();

      // if action's state not set, assume it succeeded when result ok
      if (_curAction->result().ok()
          && maintenance::FAILED != _curAction->getState()) {
//        _curAction->incStats();
        _curAction->endStats();
        _curAction->setState(maintenance::COMPLETE);

        // continue execution with "next" action tied to this one
        if (_curAction->getPostAction()) {
          _curAction = _curAction->getPostAction();
          _curAction->clearPreAction();
          _loopState = (maintenance::WAITING == _curAction->getState() ? eRUN_NEXT : eRUN_FIRST);
          _curAction->setState(maintenance::EXECUTING);
        } else {
          _curAction.reset();
          _loopState = (_directAction ? eSTOP : eFIND_ACTION);
        } // else
      } else {
        std::shared_ptr<Action> failAction(_curAction);

        // fail all actions that would follow
        do {
          failAction->setState(maintenance::FAILED);
          failAction->endStats();
          failAction=failAction->getPostAction();
        } while(failAction);
        _loopState = (_directAction ? eSTOP : eFIND_ACTION);
      } // else
    } else {
      // no current action, go back to hunting for one
      _loopState = (_directAction ? eSTOP : eFIND_ACTION);
    } // else
  } // else


} // MaintenanceWorker::nextState

#if 0
std::shared_ptr<Action> MaintenanceWorker::findReadyAction() {
  std::shared_ptr<Action> ret_ptr;



  return ret_ptr;

} // Maintenance::findReadyAction
#endif
} // namespace maintenance
} // namespace arangodb
