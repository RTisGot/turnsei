import bpy
import os

root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(
    filepath=os.path.join(root, "Resource", "Character_Gameplay.fbx"),
)

for obj in bpy.context.scene.objects:
    if obj.type != "MESH":
        continue
    unweighted = 0
    influences = {}
    for vertex in obj.data.vertices:
        total = sum(group.weight for group in vertex.groups)
        if total < 0.0001:
            unweighted += 1
        for assignment in vertex.groups:
            name = obj.vertex_groups[assignment.group].name
            influences[name] = influences.get(name, 0.0) + assignment.weight
    strongest = sorted(
        influences.items(), key=lambda item: item[1], reverse=True)[:8]
    print(
        "EXPORTED_SKIN",
        obj.name,
        "vertices=" + str(len(obj.data.vertices)),
        "unweighted=" + str(unweighted),
        "top=" + ",".join(
            name + ":" + f"{weight:.1f}" for name, weight in strongest),
    )
    unweighted_ids = {
        vertex.index for vertex in obj.data.vertices
        if sum(group.weight for group in vertex.groups) < 0.0001
    }
    by_material = {}
    for polygon in obj.data.polygons:
        count = sum(index in unweighted_ids for index in polygon.vertices)
        if count:
            material_name = (
                obj.material_slots[polygon.material_index].name
                if polygon.material_index < len(obj.material_slots)
                else "<none>"
            )
            by_material[material_name] = by_material.get(material_name, 0) + count
    print("UNWEIGHTED_MATERIALS", sorted(
        by_material.items(), key=lambda item: item[1], reverse=True))
