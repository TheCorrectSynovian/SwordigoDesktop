# Protobuf Schema Notes - 2026-06-14 - Agent 1

These notes are from `protoc --decode_raw`; field names are provisional unless supported by native symbols or obvious string content. Do not promote guessed field names without evidence.

## `gamedata.gdata`

- Format: protobuf wire format. Confidence: High.
- Top-level field `1`: repeated item/data record. Confidence: High.
- Inside top-level field `1`:
  - Field `1`: numeric category/type. Values observed in the first sample records: `2`, `3`, `4`. Confidence: Medium.
  - Field `2`: resource/internal id string, e.g. `brasssword`, `needle`, `broadsword`, `firetrinket`. Confidence: High.
  - Field `3`: display name string, e.g. `Brass Sword`, `The Needle`, `Trinket of Fire`. Confidence: High.
  - Field `4`: short stat text, e.g. `+0 damage`, `50% damage reduction`, `-`. Confidence: High.
  - Field `5`: description string. Confidence: High.
  - Field `6`: boolean or availability flag; observed `1` in sampled records. Confidence: Low.
  - Fields `7`, `8`, `9`: numeric gameplay/item metadata. Confidence: Low.

Evidence: `assets/resources/gamedata.gdata`; `research/experiments/experiment_001.md`.

## `*.scene`

- Format: protobuf wire format. Confidence: High.
- Top-level field `1`: repeated scene object. Confidence: High.
- Scene object field `2`: object name, e.g. `Background`, `DirectionalLight`, `darkhero`. Confidence: High.
- Scene object field `3`: repeated component/subobject messages. Confidence: Medium-high.
- Component field `1`: component type/name string in many decoded records, e.g. `Background`, `Light`, `KeyframeAnimation`. Confidence: Medium-high.
- Component field `2`: numeric id or type id. Confidence: Low.
- High-numbered fields such as `101`, `102`, `130`, and `200`: likely protobuf extensions for concrete component payloads. Confidence: Medium because local native symbols include many `Caver::Proto::*` component symbols.
- Scene object fields `4` through `8`: transform/bounds-like numeric messages; several values decode as IEEE754 floats. Confidence: Medium.
- Lua/program data is present in at least some `.scene` files. `assets/resources/plains_woodkeep3.scene` contains readable Lua source such as `Program.Wait(...)`, `EntityController.SetMoveSpeed(...)`, and bytecode-like chunks beginning with `LuaQ`. Exact protobuf field numbers for script source/bytecode are not confirmed in this document yet. Confidence: High for Lua presence, low for field mapping.

Evidence: `assets/resources/menu.scene`; `assets/resources/plains_woodkeep3.scene`; `research/experiments/experiment_001.md`.

## `*.gstate`

- Format: protobuf wire format. Confidence: High.
- `newplayer.gstate` sample fields:
  - Field `1`: empty string.
  - Field `3`: start scene string `town_herohouse`.
  - Field `4`: spawn point string `spawn_default`.
  - Field `9`: map/menu state string `map`.
- `player.gstate` sample fields:
  - Field `1`: nested player/profile state message.
  - Field `1.5`: numeric value `598`. Meaning unknown.
  - Field `1.11`: repeated item strings including `legendsword`, `platearmor`, `shadowtrinket`, `icetrinket`, `firetrinket`, and `iselon_shard_*`.
  - Field `1.15`: repeated ability strings including `bolt`, `bomb`, `hookshot`, and `dimension`.
  - Field `1.16`: selected ability string `hookshot`.
  - Field `1.17`: selected modifier/equipment string `firetrinket`.
  - Fields `3`, `4`, `9`: same start scene/spawn/map strings as `newplayer.gstate`.

Evidence: `assets/resources/player.gstate`, `assets/resources/newplayer.gstate`; `research/experiments/experiment_001.md`.

## `*.scl` (Updated 2026-06-14 - Agent 2)

- Format: protobuf wire format. Confidence: High.
- Field `1`: Collection name (string), e.g., `"monsters"`, `"caves_stuff"`. Confidence: High.
- Field `2`: Repeated entity class definitions. Confidence: High.
- Entity record (Field `2`):
  - Field `1`: Nested class info?
  - Field `2`: Class name (string), e.g., `"Cave Bat"`, `"Blob"`. Confidence: High.
  - Field `3`: Repeated component definitions. Confidence: High.

Evidence: `protoc --decode_raw < "assets/resources/monsters.scl"`.

## Lua Scripting Fields (Updated 2026-06-14 - Agent 2)

Embedded scripts in `.scene` files appear in high-numbered extension fields within components.

- Observed in `Program` or trigger components:
  - Field `157.2.1` or `121.9.1`: Lua source code (string).
  - Field `157.2.2` or `121.9.2`: Lua bytecode (`LuaQ` / 5.1).

Confidence: High for existence, Medium for field mapping (mapping varies by component type).

Evidence: `protoc --decode_raw < "assets/resources/plains_woodkeep3.scene"`.
