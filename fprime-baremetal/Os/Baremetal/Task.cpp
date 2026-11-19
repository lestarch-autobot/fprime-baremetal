// ======================================================================
// \title Os/Baremetal/Task.cpp
// \brief baremetal implementations for Os::Baremetal::Task
// ======================================================================
#include "fprime-baremetal/Os/Baremetal/Task.hpp"
#include "fprime-baremetal/Os/TaskRunner/TaskRunner.hpp"  // Required for task execution
namespace Os {
namespace Baremetal {

BaremetalTask::BaremetalTask() {
    (void)TaskRunner::getSingleton();  // Force the task runner into existence
}

BaremetalTask::~BaremetalTask() {
    this->deregister(this);
}

void BaremetalTask::onStart() {}

Os::TaskInterface::Status BaremetalTask::join() {
    if (not this->prepareJoin(this)) {
        return Os::TaskInterface::Status::INVALID_STATE;
    }
    const Status status = Os::TaskInterface::Status::OP_OK;
    this->completeJoin(this, status);
    return status;
}

void BaremetalTask::suspend(Os::TaskInterface::SuspensionType suspensionType) {
    this->m_handle.m_enabled = false;
    this->completeSuspend(this, suspensionType);
}

void BaremetalTask::resume() {
    this->m_handle.m_enabled = true;
}

Os::TaskHandle* BaremetalTask::getHandle() {
    return &m_handle;
}

Os::TaskInterface::Status BaremetalTask::start(const Os::TaskInterface::Arguments& arguments) {
    FW_ASSERT(arguments.m_routine != nullptr);
    const Arguments wrapped_arguments = this->prepareStart(this, arguments);
    // Set handle member variables
    this->m_handle.m_enabled = true;
    this->m_handle.m_routine = wrapped_arguments.m_routine;
    this->m_handle.m_argument = wrapped_arguments.m_routine_argument;

    // Running the task the first time allows setup activities for the task
    this->m_handle.m_routine(this->m_handle.m_argument);

    const Status status = Os::TaskInterface::Status::OP_OK;
    this->completeStart(this, status, wrapped_arguments);
    return status;
}

bool BaremetalTask::isCooperative() {
    return true;
}

Os::TaskInterface::Status BaremetalTask::_delay(const Fw::TimeInterval& interval) {
    // It is an error to attempt to delay a baremetal task because this would amount to [insert something bad here]
    FW_ASSERT(0);
    return Os::TaskInterface::Status::UNKNOWN_ERROR;
}

}  // namespace Baremetal
}  // namespace Os
