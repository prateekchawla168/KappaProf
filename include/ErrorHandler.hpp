#pragma once

#include <cstring>  // for strerror
#include <string>

namespace KProf {
// Handles enumeration and display of perf errors

std::string DescribeError(size_t errnum) {
  auto res = std::string("");
  switch (errnum) {
    case E2BIG:
      res.append(
          "The perf_event_attr size value is too small (smaller "
          "than PERF_ATTR_SIZE_VER0), too big (larger than the page size), or "
          "larger than the kernel supports and the extra bytes are not zero. "
          "The perf_event_attr size field is overwritten by the kernel to be "
          "the size of the structure it was expecting.");
      break;
    case EACCES:
      res.append(
          "The requested event requires CAP_PERFMON (since Linux 5.8) or "
          "CAP_SYS_ADMIN permissions (or a more permissive perf_event "
          "paranoid setting). Some common cases where an unprivileged process "
          "may encounter this error: attaching to a process owned by a "
          "different user; monitoring all processes on a given CPU (i.e., "
          "specifying the pid argument as -1); and not setting exclude_kernel "
          "when the paranoid setting requires it.");
      break;

    case EBADF:
      res.append(
          "The group_fd file descriptor is not valid, or if "
          "PERF_FLAG_PID_CGROUP "
          "is set, the cgroup file descriptor in pid is not valid.");
      break;
    case EBUSY:
      res.append(
          "Another event already has exclusive access to the PMU (since Linux "
          "4.1).");
      break;
    case EFAULT:
      res.append("The attr pointer points at an invalid memory address.");
      break;
    case EINTR:
      res.append("Trying to mix perf and ftrace handling for a uprobe.");
      break;
    case EINVAL:
      res.append(
          "The specified event is invalid. There are many possible reasons for "
          "this. A not-exhaustive list: "
          "\n-\tsample_freq is higher than the maximum setting,"
          "\n-\t The cpu to monitor does not exist,"
          "\n-\t `read_format` is out of range,"
          "\n-\t `sample_type` is out of range,"
          "\n-\t The flags value is out of range"
          "\n-\t Exclusive or pinned set and the event is not a group leader,"
          "\n-\t The event config values are out of range or set reserved  bits"
          "\n-\t The generic event selected is not supported,"
          "\n-\t There is not enough room to add the selected event.");
      break;
    case EMFILE:
      res.append(
          "Each opened event uses one file descriptor. If a large number of "
          "events are opened, the per-process limit on the number of open file "
          "descriptors will be reached, and no more events can be created.");
      break;
    case ENODEV:
      res.append(
          "The event involves a feature not supported by the current CPU.");
      break;
    case ENOENT:
      res.append(
          "The `type` setting is not valid. This error is also returned for "
          "some unsupported generic events.");
      break;
    case ENOSPC:
      res.append(
          "Prior to Linux 3.3, if there was not enough room for the event, "
          "ENOSPC was returned. In Linux 3.3, this was changed to EINVAL. "
          "ENOSPC is still returned if you try to add more breakpoint events "
          "than supported by the hardware.");
      break;
    case ENOSYS:
      res.append(
          "`PERF_SAMPLE_STACK_USER` is set in sample_type and it is "
          "not supported by hardware.");
      break;
    case EOPNOTSUPP:
      res.append(
          "An event requiring a specific hardware feature is requested but "
          "there is no hardware support. This includes "
          "requesting low-skid events if not supported, branch tracing if it "
          "is not available, sampling if no PMU interrupt is available, and "
          "branch stacks for software events.");
      break;
    case EOVERFLOW:
      res.append(
          "`PERF_SAMPLE_CALLCHAIN` is requested and sample_max_stack "
          "is larger than the maximum specified in "
          "`/proc/sys/kernel/perf_event_max_stack`.");
      break;
    case EPERM:
      res.append(
          "An unsupported `exclude_hv`, `exclude_idle`, `exclude_user`, or "
          "`exclude_kernel` setting is specified. It can also happen, as with "
          "EACCES, when the requested event requires CAP_PERFMON (since Linux "
          "5.8) or CAP_SYS_ADMIN permissions (or a more permissive perf_event "
          "paranoid setting). This includes setting a breakpoint on a kernel "
          "address, and (since Linux 3.13) setting a kernel function-trace "
          "tracepoint. Returned on many (not all) architectures");
      break;
    case ESRCH:
      res.append("Attempting to attach to a process that does not exist.");
      break;
    default:
      res.append(strerror(errnum));
      break;
  }

  return res;
}

std::string DescribeError_IOCTL(int errnum) {
  auto res = std::string("");
  switch (errnum) {
    case EBADF:
      res.append("fd is not a valid file descriptor.");
      break;

    case EFAULT:
      res.append("argp references an inaccessible memory area.");
      break;

    case EINVAL:
      res.append("op or argp is not valid.");
      break;

    case ENOTTY:
      res.append(
          "fd is not associated with a character special device. Also returned "
          "when the specified operation does not apply to the kind of object "
          "that the file descriptor fd references.");
      break;

    default:
      res.append(strerror(errnum));
      break;
  }

  return res;
}

};  // namespace KProf