# Swordigo POD Pipeline

## 1. Overview
The game uses `.POD` files for 3D geometry and animations. These are **PowerVR Object Data** files (version 2.0).

## 2. Inventory of POD Assets
Based on file listing and metadata, there are several types of POD files:
- **Character/NPC Models**: `hiro.POD`, `npc_male1.POD`, `skelly.POD`.
- **Animations**: `hiro_run.POD`, `hiro_jump.POD`, `skelly_walk.POD`.
- **Environmental Props**: `bush.POD`, `chest.POD`, `house.POD`, `rock1.POD`.
- **Items**: `brass_sword.POD`, `item_platearmor.POD`.

## 3. Data Distribution
- **Geometry**: The main `.POD` for an entity (e.g., `hiro.POD`) contains the mesh and skeleton data.
- **Animations**: Animations are stored in **separate `.POD` files** (e.g., `hiro_run.POD`, `hiro_jump.POD`). The engine likely blends or switches between these at runtime.
- **Textures**: Textures are **external**. POD files do not contain image data. They reference texture names (e.g., `char_beta2_2x`) which correspond to `.pvr` files in the same directory.
- **References**: 
  - Entities in `.scl` files reference the base POD model and its associated animation PODs.
  - `.scene` files reference the entity templates from `.scl`, effectively placing the POD models in the world.

## 4. Metadata Analysis (AB.POD.2.0)
Header strings reveal the export settings:
- `bIndexed=1`: Meshes use indexed vertices.
- `exportMaterials=1`: Material properties are defined in the POD.
- `exportGeom=1`: Geometry is present.
- `dwBoneLimit=100`: High bone limit for skeletal animations.

## 5. Pipeline Map
Raw Assets (.pvr, .POD)
      |
      v
Entity Template (.scl)  <-- [Links Mesh POD to Animation PODs]
      |
      v
   Scene (.scene)       <-- [Places Entity Instance]
      |
      v
 Native Engine          <-- [Loads via Caver::PODLoader]
      |
      v
   GLES 1.1             <-- [Renders Mesh]
