
//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasily Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchFeature.h"
#include "IResearchView.h"
#include "ApplicationServerHelper.h"

#include "RestServer/ViewTypesFeature.h"

#include "Aql/AqlValue.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/Function.h"

#include "Logger/Logger.h"
#include "Logger/LogMacros.h"

#include "Basics/SmallVector.h"

NS_BEGIN(arangodb)

NS_BEGIN(transaction)
class Methods;
NS_END // transaction

NS_BEGIN(basics)
class VPackStringBufferAdapter;
NS_END // basics

NS_BEGIN(aql)
class Query;
NS_END // aql

NS_END // arangodb

NS_LOCAL

static std::string const FEATURE_NAME("IResearch");

arangodb::aql::AqlValue noop(
    arangodb::aql::Query*,
    arangodb::transaction::Methods* ,
    arangodb::SmallVector<arangodb::aql::AqlValue> const&) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
    TRI_ERROR_NOT_IMPLEMENTED,
    "Function is designed to use with IResearchView only"
  );
}

void registerFilters(arangodb::aql::AqlFunctionFeature& functions) {
  arangodb::iresearch::addFunction(functions, arangodb::aql::Function{
    "EXISTS",      // external name (AQL function external names are always in upper case)
    "exists",      // internal name
    ".|.,.",       // positional arguments (attribute, [ "analyzer"|"type", analyzer-name|"string"|"numeric"|"bool"|"null" ])
    false,         // cacheable
    true,          // deterministic
    true,          // can throw
    true,          // can be run on server
    true,          // can pass arguments by reference
    &noop          // function implementation (use function name as placeholder)
  });

  arangodb::iresearch::addFunction(functions, arangodb::aql::Function{
    "STARTS_WITH", // external name (AQL function external names are always in upper case)
    "starts_with", // internal name
    ".,.|.",       // positional arguments (attribute, prefix, scoring-limit)
    false,         // cacheable
    true,          // deterministic
    true,          // can throw
    true,          // can be run on server
    true,          // can pass arguments by reference
    &noop          // function implementation (use function name as placeholder)
  });

  arangodb::iresearch::addFunction(functions, arangodb::aql::Function{
    "PHRASE",      // external name (AQL function external names are always in upper case)
    "phrase",      // internal name
    ".,.|.+",      // positional arguments (attribute, input [, offset, input... ] [, analyzer])
    false,         // cacheable
    false,         // deterministic
    true,          // can throw
    true,          // can be run on server
    true,          // can pass arguments by reference
    &noop          // function implementation (use function name as placeholder)
  });
}

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

/* static */ std::string const& IResearchFeature::name() {
  return FEATURE_NAME;
}

IResearchFeature::IResearchFeature(arangodb::application_features::ApplicationServer* server)
  : ApplicationFeature(server, IResearchFeature::name()),
    _running(false) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("ViewTypes");
  startsAfter("Logger");
  startsAfter("Database");
  startsAfter("IResearchAnalyzer"); // used for retrieving IResearch analyzers for functions
  startsAfter("AQLFunctions");
  // TODO FIXME: we need the MMFilesLogfileManager to be available here if we
  // use the MMFiles engine. But it does not feel right to have such storage engine-
  // specific dependency here. Better create a "StorageEngineFeature" and make 
  // ourselves start after it!
  startsAfter("MMFilesLogfileManager");
  startsAfter("TransactionManager");

  startsBefore("GeneralServer");
}

void IResearchFeature::beginShutdown() {
  _running = false;
  ApplicationFeature::beginShutdown();
}

void IResearchFeature::collectOptions(
    std::shared_ptr<arangodb::options::ProgramOptions> options
) {
  _running = false;
  ApplicationFeature::collectOptions(options);
}

void IResearchFeature::prepare() {
  _running = false;
  ApplicationFeature::prepare();

  // load all known codecs
  ::iresearch::formats::init();

  // register 'iresearch' view
  ViewTypesFeature::registerViewImplementation(
     IResearchView::type(),
     IResearchView::make
  );
}

void IResearchFeature::start() {
  ApplicationFeature::start();

  // register IResearchView filters
 {
    auto* functions = getFeature<arangodb::aql::AqlFunctionFeature>("AQLFunctions");

    if (functions) {
      registerFilters(*functions);
    } else {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "failure to find feature 'AQLFunctions' while registering iresearch filters";
    }
  }

  _running = true;
}

void IResearchFeature::stop() {
  _running = false;
  ApplicationFeature::stop();
}

void IResearchFeature::unprepare() {
  _running = false;
  ApplicationFeature::unprepare();
}

void IResearchFeature::validateOptions(
    std::shared_ptr<arangodb::options::ProgramOptions> options
) {
  _running = false;
  ApplicationFeature::validateOptions(options);
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------