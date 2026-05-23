#include <shared/game/PhaseShift.h>

namespace Firelands {

namespace {

bool HasFlag(uint32 flags, PhaseShiftFlags bit) {
  return (flags & static_cast<uint32>(bit)) != 0u;
}

void SetFlag(uint32 &flags, PhaseShiftFlags bit) {
  flags |= static_cast<uint32>(bit);
}

void ClearFlag(uint32 &flags, PhaseShiftFlags bit) {
  flags &= ~static_cast<uint32>(bit);
}

bool PhasesIntersect(PhaseShift const &a, PhaseShift const &b,
                     uint32 excludePhaseFlags) {
  for (PhaseRef const &lhs : a.phases) {
    if ((lhs.phaseFlags & excludePhaseFlags) != 0u)
      continue;
    for (PhaseRef const &rhs : b.phases) {
      if ((rhs.phaseFlags & excludePhaseFlags) != 0u)
        continue;
      if (lhs.id == rhs.id)
        return true;
    }
  }
  return false;
}

bool InverseCanSee(PhaseShift const &viewer, PhaseShift const &subject) {
  if (HasFlag(viewer.flags, PhaseShiftFlags::Unphased) &&
      HasFlag(subject.flags, PhaseShiftFlags::InverseUnphased)) {
    return false;
  }
  for (PhaseRef const &phase : viewer.phases) {
    bool found = false;
    for (PhaseRef const &other : subject.phases) {
      if (phase.id == other.id) {
        found = true;
        break;
      }
    }
    if (found)
      return false;
  }
  return true;
}

} // namespace

void PhaseShift::ClearPhases() { phases.clear(); }

void PhaseShift::AddPhase(uint16 phaseId, uint32 phaseFlags) {
  if (phaseId == 0)
    return;
  if (std::any_of(phases.begin(), phases.end(),
                  [phaseId](PhaseRef const &p) { return p.id == phaseId; })) {
    return;
  }
  phases.push_back(PhaseRef{phaseId, phaseFlags});
}

bool PhaseShift::HasPhase(uint16 phaseId) const {
  return std::any_of(phases.begin(), phases.end(),
                     [phaseId](PhaseRef const &p) { return p.id == phaseId; });
}

bool PhaseShift::CanSee(PhaseShift const &other) const {
  if (HasFlag(flags, PhaseShiftFlags::Unphased) &&
      HasFlag(other.flags, PhaseShiftFlags::Unphased)) {
    return true;
  }
  if (HasFlag(flags, PhaseShiftFlags::AlwaysVisible) ||
      HasFlag(other.flags, PhaseShiftFlags::AlwaysVisible)) {
    return true;
  }
  if (HasFlag(flags, PhaseShiftFlags::Inverse) &&
      HasFlag(other.flags, PhaseShiftFlags::Inverse)) {
    return true;
  }

  uint32 const excludeFlags = 0u;
  if (!HasFlag(flags, PhaseShiftFlags::Inverse) &&
      !HasFlag(other.flags, PhaseShiftFlags::Inverse)) {
    return PhasesIntersect(*this, other, excludeFlags);
  }
  if (HasFlag(other.flags, PhaseShiftFlags::Inverse)) {
    return InverseCanSee(*this, other);
  }
  return InverseCanSee(other, *this);
}

void InitDbCreaturePhaseShift(
    PhaseShift &out, uint8 phaseUseFlags, uint16 phaseId, uint32 phaseGroupId,
    std::function<std::vector<uint16>(uint32 phaseGroupId)> const &resolveGroup) {
  out.ClearPhases();
  out.personalGuid = 0;
  out.flags = static_cast<uint32>(PhaseShiftFlags::None);

  if (phaseUseFlags & kPhaseUseFlagsAlwaysVisible) {
    SetFlag(out.flags, PhaseShiftFlags::AlwaysVisible);
    SetFlag(out.flags, PhaseShiftFlags::Unphased);
  }
  if (phaseUseFlags & kPhaseUseFlagsInverse) {
    SetFlag(out.flags, PhaseShiftFlags::Inverse);
  }

  if (phaseId != 0) {
    out.AddPhase(phaseId);
  } else if (phaseGroupId != 0 && resolveGroup) {
    for (uint16 const id : resolveGroup(phaseGroupId))
      out.AddPhase(id);
  }

  if (out.phases.empty() || out.HasPhase(kDefaultPhaseId)) {
    if (HasFlag(out.flags, PhaseShiftFlags::Inverse)) {
      SetFlag(out.flags, PhaseShiftFlags::InverseUnphased);
    } else {
      SetFlag(out.flags, PhaseShiftFlags::Unphased);
    }
  }
}

} // namespace Firelands
