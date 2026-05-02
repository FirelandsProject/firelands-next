-- Remove starter spell ids that are not present in 4.3.4 client Spell.dbc.
-- They can prevent the client from applying the rest of `SMSG_SEND_KNOWN_SPELLS`,
-- so language passives never register and `/say` never sends `CMSG_MESSAGECHAT_SAY`.
DELETE FROM `firelands_world`.`playercreateinfo_spell`
WHERE `spellId` IN (86470, 86471, 86473, 86475, 86478);
