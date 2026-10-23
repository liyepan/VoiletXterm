#pragma once

#include "SessionSpec.h"

namespace vx {

class Transport;

Transport *createTransport(const SessionSpec &spec, QObject *parent);

} // namespace vx
