#pragma once

#include <map>
#include <vector>
#include <shared/network/BitWriter.h>
#include <shared/network/ByteBuffer.h>
#include <shared/network/MovementInfo.h>
#include <shared/network/UpdateFields.h>
#include <shared/network/WorldPacket.h>
#include <vector>

namespace Firelands {

/**
 * @brief Accumulates object updates to be sent in a single SMSG_UPDATE_OBJECT
 * packet. Specific to Cataclysm 4.3.4 format.
 */
class UpdateData {
public:
  UpdateData() : _count(0), _mapId(0) {}
  explicit UpdateData(uint16 mapId) : _count(0), _mapId(mapId) {}

  // Update flags (Cataclysm 4.3.4 / build 15595) aligned with FirelandsCore reference.
  // We only define the ones we currently emit.
  static constexpr uint32 UPDATEFLAG_SELF = 0x00001;
  static constexpr uint32 UPDATEFLAG_LIVING = 0x00020;

  /**
   * @brief Adds a CreateObject block to the update data.
   * @param guid The GUID of the object being created.
   * @param typeId The TypeID of the object.
   * @param move Movement information.
   * @param fields Update fields.
   */
  void AddCreateObject(uint64 guid, TypeID typeId, MovementInfo const &move,
                       std::map<uint16, uint32> const &fields) {
    _count++;
    _data.Append<uint8>(UPDATETYPE_CREATE_OBJECT2);
    _data.WritePackedGuid(guid);
    _data.Append<uint8>(static_cast<uint8>(typeId));

    // Cataclysm 4.3.4 (15595) movement update block.
    // We currently build the minimal "self living unit" movement update matching FirelandsCore.
    uint32 flags = 0;
    if (typeId == TYPEID_PLAYER)
      flags = UPDATEFLAG_SELF | UPDATEFLAG_LIVING;
    else if (typeId == TYPEID_UNIT)
      flags = UPDATEFLAG_LIVING;

    uint8 guidBytes[8];
    for (int i = 0; i < 8; ++i)
      guidBytes[i] = (guid >> (i * 8)) & 0xFF;

    bool isZeroOrientation = (move.orientation == 0.0f);
    uint32 movementFlags0 = 0;
    uint16 movementFlags1 = 0;
    bool hasPitch = false;
    bool hasFallData = false;
    bool hasSpline = false;
    bool hasSplineElevation = false;
    bool hasTransport = false;

    BitWriter bw(_data);
    bw.WriteBit(false); // UPDATEFLAG_PLAY_HOVER_ANIM
    bw.WriteBit(false); // UPDATEFLAG_SUPPRESSED_GREETINGS
    bw.WriteBit(false); // UPDATEFLAG_ROTATION
    bw.WriteBit(false); // UPDATEFLAG_ANIMKITS
    bw.WriteBit(false); // UPDATEFLAG_HAS_TARGET
    bw.WriteBit((flags & UPDATEFLAG_SELF) != 0);
    bw.WriteBit(false); // UPDATEFLAG_VEHICLE
    bw.WriteBit((flags & UPDATEFLAG_LIVING) != 0);
    bw.WriteBits(0, 24); // PauseTimes size
    bw.WriteBit(false);  // UPDATEFLAG_NO_BIRTH_ANIM
    bw.WriteBit(false);  // UPDATEFLAG_GO_TRANSPORT_POSITION
    bw.WriteBit(false);  // UPDATEFLAG_STATIONARY_POSITION
    bw.WriteBit(false);  // UPDATEFLAG_AREATRIGGER
    bw.WriteBit(false);  // UPDATEFLAG_ENABLE_PORTALS
    bw.WriteBit(false);  // UPDATEFLAG_TRANSPORT

    if (flags & UPDATEFLAG_LIVING) {
      bw.WriteBit(!movementFlags0); // !Has MoveFlags0
      bw.WriteBit(isZeroOrientation);
      bw.WriteBit(guidBytes[7] != 0);
      bw.WriteBit(guidBytes[3] != 0);
      bw.WriteBit(guidBytes[2] != 0);
      // movementFlags0 == 0 => no bits written

      bw.WriteBit(false);     // !Has player spline data (hasSpline && !isPlayer)
      bw.WriteBit(!hasPitch); // !Has pitch
      bw.WriteBit(hasSpline); // Has spline data
      bw.WriteBit(hasFallData);
      bw.WriteBit(!hasSplineElevation); // !Has spline elevation
      bw.WriteBit(guidBytes[5] != 0);
      // Reference (FirelandsCore): this bit is true only when transport guid is present.
      bw.WriteBit(hasTransport); // Has transport data
      bw.WriteBit(false);         // !HasTime (always send time in bytes)

      bw.WriteBit(guidBytes[4] != 0);
      bw.WriteBit(guidBytes[6] != 0);
      bw.WriteBit(guidBytes[0] != 0);
      bw.WriteBit(guidBytes[1] != 0);
      bw.WriteBit(false);           // HeightChangeFailed
      bw.WriteBit(!movementFlags1); // !Has MoveFlags1
      // movementFlags1 == 0 => no bits written
    }

    bw.Flush();

    if (flags & UPDATEFLAG_LIVING) {
      // Byte order must match Object::BuildMovementUpdate (FirelandsCore
      // Object.cpp) for this flag combination: no fall, no spline elevation,
      // no spline, no transport, no pitch, orientation omitted when zero.
      _data.WriteByteSeq(guidBytes[4]);
      _data.Append<float>(4.5f); // MOVE_RUN_BACK
      _data.Append<float>(4.5f); // MOVE_SWIM_BACK
      _data.Append<float>(move.z);
      _data.WriteByteSeq(guidBytes[5]);
      // transport block omitted (no transport guid)
      _data.Append<float>(move.x);
      _data.Append<float>(3.14159f); // MOVE_PITCH_RATE
      _data.WriteByteSeq(guidBytes[3]);
      _data.WriteByteSeq(guidBytes[0]);
      _data.Append<float>(4.72222f); // MOVE_SWIM
      _data.Append<float>(move.y);
      _data.WriteByteSeq(guidBytes[7]);
      _data.WriteByteSeq(guidBytes[1]);
      _data.WriteByteSeq(guidBytes[2]);
      _data.Append<float>(2.5f); // MOVE_WALK
      _data.Append<uint32>(static_cast<uint32>(move.time));
      _data.Append<float>(3.14159f); // MOVE_TURN_RATE
      _data.WriteByteSeq(guidBytes[6]);
      _data.Append<float>(7.0f); // MOVE_FLIGHT
      if (!isZeroOrientation)
        _data.Append<float>(move.orientation);
      _data.Append<float>(7.0f); // MOVE_RUN
      _data.Append<float>(4.5f); // MOVE_FLIGHT_BACK
    }

    AppendUpdateFieldValues(fields);
  }

  /// `UPDATETYPE_OUT_OF_RANGE_OBJECTS` — removes object visibility for nearby clients.
  void AddOutOfRangeObjects(std::vector<uint64> const &guids) {
    if (guids.empty())
      return;
    _count++;
    _data.Append<uint8>(UPDATETYPE_OUT_OF_RANGE_OBJECTS);
    _data.Append<uint32>(static_cast<uint32>(guids.size()));
    for (uint64 const g : guids)
      _data.WritePackedGuid(g);
  }

  /// `UPDATETYPE_VALUES` block (no movement). Used for inventory slot refreshes after swaps.
  void AddValuesUpdate(uint64 guid, std::map<uint16, uint32> const &fields) {
    _count++;
    _data.Append<uint8>(UPDATETYPE_VALUES);
    _data.WritePackedGuid(guid);
    AppendUpdateFieldValues(fields);
  }

  /**
   * @brief Builds the SMSG_UPDATE_OBJECT packet.
   * @param packet The WorldPacket to append the data to.
   */
  void Build(WorldPacket &packet) {
    packet.SetOpcode(SMSG_UPDATE_OBJECT);
    // FirelandsCore (Cataclysm 4.3.4): [2] mapId + [4] blockCount, then blocks.
    packet.Append<uint16>(_mapId);
    packet.Append<uint32>(_count);
    packet.Append(_data.GetBuffer(), _data.Size());
  }

  size_t GetBlockCount() const { return _count; }

private:
  static void AppendUpdateFieldValues(ByteBuffer &buf,
                                      std::map<uint16, uint32> const &fields) {
    uint32 maxField = 0;
    if (!fields.empty())
      maxField = fields.rbegin()->first + 1;

    uint32 maskSize = (maxField + 31) / 32;
    buf.Append<uint8>(static_cast<uint8>(maskSize));

    if (maskSize == 0)
      return;

    std::vector<uint32> mask(maskSize, 0);
    for (auto const &[index, value] : fields) {
      mask[index / 32] |= (1u << (index % 32));
    }

    for (uint32 m : mask)
      buf.Append<uint32>(m);

    for (uint32 i = 0; i < maxField; ++i) {
      if (mask[i / 32] & (1u << (i % 32)))
        buf.Append<uint32>(fields.at(i));
    }
  }

  void AppendUpdateFieldValues(std::map<uint16, uint32> const &fields) {
    AppendUpdateFieldValues(_data, fields);
  }

  uint32 _count;
  uint16 _mapId;
  ByteBuffer _data;
};

} // namespace Firelands
