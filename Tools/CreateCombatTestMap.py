"""Run in UE Python (Python Editor Script Plugin required). Creates NEW assets only.
Does not modify L_Base, default project maps, existing blueprints, or existing assets.
"""
import unreal
MAP = "/Game/Loong/Maps/L_Combat"
CONFIG = "/Game/Loong/Combat/Test/DA_CombatDemo"
# Preflight rejects overwrite before any mutation.
for path in (MAP, CONFIG):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        raise RuntimeError("Refusing to overwrite existing asset: " + path)
worlds = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
current_world = worlds.get_editor_world()
if current_world:
    dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    if dirty:
        raise RuntimeError("Save the current map first. Script will not replace a dirty map.")
combat_class = unreal.load_class(None, "/Script/Loong.LoongCombatGameMode")
definition_class = unreal.load_class(None, "/Script/Loong.LoongCombatDefinition")
if not combat_class or not definition_class:
    raise RuntimeError("Compile Loong C++ and reopen editor first.")
factory = unreal.DataAssetFactory()
factory.set_editor_property("data_asset_class", definition_class)
config = unreal.AssetToolsHelpers.get_asset_tools().create_asset("DA_CombatDemo", "/Game/Loong/Combat/Test", definition_class, factory)
if not config:
    raise RuntimeError("Unable to create configuration asset.")
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not levels.new_level(MAP):
    raise RuntimeError("Unable to create new map. Configuration may have been created; nothing was overwritten.")
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
cube = unreal.load_asset("/Engine/BasicShapes/Cube")
def block(label, position, scale):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*position))
    actor.set_actor_label(label)
    actor.static_mesh_component.set_static_mesh(cube)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor
block("CombatGround", (0, 0, -25), (60, 60, 0.5))
# Four boundaries keep the current player movement within the arena.
for label, pos, scale in (
    ("NorthWall", (0,3000,100), (60,0.5,3)),
    ("SouthWall", (0,-3000,100), (60,0.5,3)),
    ("EastWall", (3000,0,100), (0.5,60,3)),
    ("WestWall", (-3000,0,100), (0.5,60,3))):
    block(label,pos,scale)
actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0,0,110))
director_class = unreal.load_class(None, "/Script/Loong.LoongCombatDirector")
director = actors.spawn_actor_from_class(director_class, unreal.Vector(0,0,0))
director.set_actor_label("CombatDirector_Configurable")
director.set_editor_property("definition", config)
actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0,0,1000), unreal.Rotator(-50,-30,0))
actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0,0,1000))
world = worlds.get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", combat_class)
# Default GameMode uses native defaults; set config in World Settings only if customizing.
unreal.EditorAssetLibrary.save_loaded_asset(config)
levels.save_current_level()
unreal.log("Created L_Combat. Native defaults provide 180s demo. Optional DA_CombatDemo can be configured for future GameMode settings.")
