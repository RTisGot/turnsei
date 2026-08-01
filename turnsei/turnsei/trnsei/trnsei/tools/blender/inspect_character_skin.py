import bpy
import os

root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
bpy.ops.wm.open_mainfile(filepath=os.path.join(root, "Resource", "Character.blend"))

for armature in (obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE"):
    for bone in armature.data.bones:
        head = armature.matrix_world @ bone.head_local
        tail = armature.matrix_world @ bone.tail_local
        if max(head.z, tail.z) >= 16.0:
            print("BONE", bone.name, f"z={head.z:.3f}..{tail.z:.3f}",
                  "deform=" + str(bone.use_deform))

for obj in bpy.context.scene.objects:
    if obj.type != "MESH" or obj.hide_get() or obj.hide_render:
        continue
    armatures = [m.object.name for m in obj.modifiers
                 if m.type == "ARMATURE" and m.object]
    weighted = sum(
        1 for vertex in obj.data.vertices
        if any(group.weight > 0.0001 for group in vertex.groups)
    )
    corners = [obj.matrix_world @ v.co for v in obj.data.vertices]
    z_min = min((v.z for v in corners), default=0.0)
    z_max = max((v.z for v in corners), default=0.0)
    group_totals = {}
    for vertex in obj.data.vertices:
        for assignment in vertex.groups:
            name = obj.vertex_groups[assignment.group].name
            group_totals[name] = group_totals.get(name, 0.0) + assignment.weight
    strongest = sorted(group_totals.items(), key=lambda item: item[1], reverse=True)[:5]
    print(
        "SKIN",
        obj.name,
        "verts=" + str(len(obj.data.vertices)),
        "weighted=" + str(weighted),
        "groups=" + str(len(obj.vertex_groups)),
        "armatures=" + ",".join(armatures),
        "z=" + f"{z_min:.3f}..{z_max:.3f}",
        "top=" + ",".join(name + ":" + f"{weight:.1f}" for name, weight in strongest),
        "materials=" + ",".join(slot.material.name if slot.material else "<none>"
                               for slot in obj.material_slots),
    )
