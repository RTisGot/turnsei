import bpy
import os
import json
import re

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SOURCE = os.path.join(ROOT, "Resource", "Character.blend")
OUTPUT = os.path.join(ROOT, "Resource", "Character_Gameplay.fbx")
ANIM_OUTPUT = os.path.join(ROOT, "Resource", "Character_Walk.json")

bpy.ops.wm.open_mainfile(filepath=SOURCE)
if bpy.context.object and bpy.context.object.mode != "OBJECT":
    bpy.ops.object.mode_set(mode="OBJECT")

export_meshes = [
    o for o in bpy.context.scene.objects
    if o.type == "MESH" and not o.hide_get() and not o.hide_render
]
armatures = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]
if not armatures:
    raise RuntimeError("Character.blend contains no armature")
armature = armatures[0]
armature.hide_set(False)
armature.hide_viewport = False
armature.hide_render = False

# Some clothing/accessory objects in the source file have no skin weights, so
# they remain at the bind position while the body walks. Transfer the body's
# nearest surface weights to those objects and bind them to the same armature.
weighted_meshes = [
    o for o in export_meshes
    if any(v.groups for v in o.data.vertices) and len(o.vertex_groups) > 0
]
if not weighted_meshes:
    raise RuntimeError("Character.blend contains no weighted body mesh")
body_mesh = max(
    weighted_meshes,
    key=lambda o: sum(1 for v in o.data.vertices if v.groups),
)

for obj in export_meshes:
    # Rebuild every garment/accessory from the body's deformation field.
    # Several source garments are only partially weighted; checking merely
    # whether an object has groups leaves loose jacket panels behind.
    if obj != body_mesh:
        # Data Transfer only writes into destination vertex groups that
        # already exist. Create the body's complete deformation-group layout
        # first, especially for the entirely unrigged outer coat object.
        for source_group in body_mesh.vertex_groups:
            if obj.vertex_groups.get(source_group.name) is None:
                obj.vertex_groups.new(name=source_group.name)
        transfer = obj.modifiers.new("Gameplay_WeightTransfer", "DATA_TRANSFER")
        transfer.object = body_mesh
        transfer.use_vert_data = True
        transfer.data_types_verts = {"VGROUP_WEIGHTS"}
        transfer.vert_mapping = "POLYINTERP_NEAREST"
        # Evaluate weight transfer before subdivision/solidify/armature.
        while obj.modifiers.find(transfer.name) > 0:
            bpy.context.view_layer.objects.active = obj
            bpy.ops.object.modifier_move_up(modifier=transfer.name)
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)
        bpy.ops.object.modifier_apply(modifier=transfer.name)
        obj.select_set(False)

        # Hair, eyes and other head-only pieces should move rigidly with the
        # head. Nearest-surface transfer can otherwise pick neck/shoulder
        # weights for long hair and make it lag behind the skull.
        world_vertices = [obj.matrix_world @ vertex.co
                          for vertex in obj.data.vertices]
        minimum_z = min((vertex.z for vertex in world_vertices), default=-1e9)
        if minimum_z >= 17.0:
            for group in list(obj.vertex_groups):
                obj.vertex_groups.remove(group)
            head_group = obj.vertex_groups.new(name="ボーン.005")
            head_group.add(
                list(range(len(obj.data.vertices))), 1.0, "REPLACE")

    # "ピン" is a Blender rigging/helper group, not a gameplay deformation
    # bone. It was taking one of the four exported influence slots and leaving
    # stretched geometry under the character's feet during the walk cycle.
    pin_group = obj.vertex_groups.get("ピン")
    if pin_group is not None:
        obj.vertex_groups.remove(pin_group)
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)
        bpy.ops.object.vertex_group_normalize_all(
            group_select_mode="ALL",
            lock_active=False,
        )
        obj.select_set(False)

    armature_modifiers = [m for m in obj.modifiers if m.type == "ARMATURE"]
    if not armature_modifiers:
        modifier = obj.modifiers.new("Gameplay_Armature", "ARMATURE")
        modifier.object = armature
    else:
        for modifier in armature_modifiers:
            modifier.object = armature

# Assimp exposes one offset matrix per bone in the runtime, while separate FBX
# mesh objects may carry different object-space bind offsets for that same
# bone. Join all character parts into one skinned object so hair/clothing and
# the body share exactly one bind space. Material slots remain intact.
for scene_object in bpy.context.scene.objects:
    scene_object.select_set(False)
for obj in export_meshes:
    obj.select_set(True)
bpy.context.view_layer.objects.active = body_mesh
bpy.ops.object.join()
body_mesh.name = "Character_Gameplay"
export_meshes = [body_mesh]
hips_group = body_mesh.vertex_groups.get("Hips")
if hips_group is None:
    hips_group = body_mesh.vertex_groups.new(name="Hips")
unweighted_vertices = [
    vertex.index for vertex in body_mesh.data.vertices
    if sum(assignment.weight for assignment in vertex.groups) < 0.0001
]
if unweighted_vertices:
    hips_group.add(unweighted_vertices, 1.0, "REPLACE")

source_walk = next((a for a in bpy.data.actions if "walk" in a.name.lower()), None)
if source_walk is None:
    raise RuntimeError("Character.blend contains no Walk action")

# The source was saved by a newer Blender with a slotted Action. Blender 4.5
# exposes its F-curves but does not evaluate that slot reliably for FBX bake.
# Sample every source curve ourselves, apply it to the pose, and key a fresh
# action that is owned by this armature.
source_curves = [
    (curve.data_path, curve.array_index,
     [curve.evaluate(frame) for frame in range(0, 25)])
    for curve in source_walk.fcurves
]

channels = {}
for data_path, array_index, samples in source_curves:
    match = re.match(r'pose\.bones\["(.+)"\]\.(location|rotation_quaternion|scale)', data_path)
    if not match:
        continue
    bone_name, prop = match.groups()
    channel = channels.setdefault(bone_name, {
        "position": [[0.0, 0.0, 0.0] for _ in range(25)],
        "rotation": [[1.0, 0.0, 0.0, 0.0] for _ in range(25)],
        "scale": [[1.0, 1.0, 1.0] for _ in range(25)],
    })
    key = {"location": "position", "rotation_quaternion": "rotation", "scale": "scale"}[prop]
    for frame, value in enumerate(samples):
        channel[key][frame][array_index] = value
with open(ANIM_OUTPUT, "w", encoding="utf-8") as file:
    json.dump({"name": "Walk_Loop", "fps": 30.0, "frames": 25, "channels": channels},
              file, ensure_ascii=False)
for action in list(bpy.data.actions):
    bpy.data.actions.remove(action)

if armature.animation_data is None:
    armature.animation_data_create()
walk = bpy.data.actions.new("Walk_Loop")
armature.animation_data.action = walk
if walk.slots:
    armature.animation_data.action_slot = walk.slots[0]
for frame in range(0, 25):
    for data_path, array_index, samples in source_curves:
        try:
            value = samples[frame]
            owner_path, property_name = data_path.rsplit(".", 1)
            owner = armature.path_resolve(owner_path)
            property_value = getattr(owner, property_name).copy()
            property_value[array_index] = value
            setattr(owner, property_name, property_value)
            armature.keyframe_insert(
                data_path=data_path, index=array_index, frame=frame)
        except (ValueError, TypeError):
            pass
bpy.context.scene.frame_set(int(walk.frame_range[0]))
bpy.context.view_layer.update()

armature.animation_data.action = walk
if walk.slots:
    armature.animation_data.action_slot = walk.slots[0]
bpy.context.scene.frame_start = int(walk.frame_range[0])
bpy.context.scene.frame_end = int(walk.frame_range[1])
bpy.context.scene.render.fps = 30

for obj in bpy.context.scene.objects:
    obj.select_set(False)
armature.select_set(True)
for obj in export_meshes:
    obj.select_set(True)

bpy.context.view_layer.objects.active = armature
bpy.ops.export_scene.fbx(
    filepath=OUTPUT,
    use_selection=True,
    object_types={"ARMATURE", "MESH"},
    apply_unit_scale=True,
    apply_scale_options="FBX_SCALE_ALL",
    axis_forward="-Z",
    axis_up="Y",
    add_leaf_bones=False,
    use_armature_deform_only=True,
    bake_anim=True,
    bake_anim_use_all_bones=True,
    bake_anim_use_nla_strips=False,
    bake_anim_use_all_actions=False,
    bake_anim_force_startend_keying=True,
    bake_anim_step=1.0,
    bake_anim_simplify_factor=0.0,
)

print("Exported gameplay character:", OUTPUT)
print("Armature bones:", len(armature.data.bones))
print("Walk range:", tuple(walk.frame_range))
