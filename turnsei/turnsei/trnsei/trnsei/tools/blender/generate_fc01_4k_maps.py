import bpy
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "Resource", "DrownedCity", "Textures", "FC01_4K")
os.makedirs(OUT, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = "BLENDER_EEVEE_NEXT"
scene.render.resolution_x = 4096
scene.render.resolution_y = 4096
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA"
scene.render.film_transparent = False
scene.view_settings.look = "AgX - Medium High Contrast"

bpy.ops.mesh.primitive_plane_add(size=2.0)
plane = bpy.context.object
mat = bpy.data.materials.new("M_FC01_BakeSource")
mat.use_nodes = True
plane.data.materials.append(mat)
nodes = mat.node_tree.nodes
links = mat.node_tree.links
nodes.clear()
out = nodes.new("ShaderNodeOutputMaterial")
emit = nodes.new("ShaderNodeEmission")
tex = nodes.new("ShaderNodeTexCoord")
mapping = nodes.new("ShaderNodeMapping")
noise_large = nodes.new("ShaderNodeTexNoise")
noise_large.inputs["Scale"].default_value = 7.0
noise_large.inputs["Detail"].default_value = 6.0
noise_large.inputs["Roughness"].default_value = 0.72
noise_fine = nodes.new("ShaderNodeTexNoise")
noise_fine.inputs["Scale"].default_value = 95.0
noise_fine.inputs["Detail"].default_value = 3.0
noise_fine.inputs["Roughness"].default_value = 0.78
mix = nodes.new("ShaderNodeMixRGB")
mix.blend_type = "MULTIPLY"
mix.inputs[0].default_value = 0.34
links.new(tex.outputs["Generated"], mapping.inputs["Vector"])
links.new(mapping.outputs["Vector"], noise_large.inputs["Vector"])
links.new(mapping.outputs["Vector"], noise_fine.inputs["Vector"])
links.new(noise_large.outputs["Fac"], mix.inputs[1])
links.new(noise_fine.outputs["Fac"], mix.inputs[2])
links.new(emit.outputs["Emission"], out.inputs["Surface"])

bpy.ops.object.camera_add(location=(0, 0, 1), rotation=(0, 0, 0))
cam = bpy.context.object
cam.data.type = "ORTHO"
cam.data.ortho_scale = 2.0
scene.camera = cam

def render_map(name, ramp_values, colorspace="sRGB"):
    ramp = nodes.get("FC01_Ramp") or nodes.new("ShaderNodeValToRGB")
    ramp.name = "FC01_Ramp"
    ramp.color_ramp.elements.remove(ramp.color_ramp.elements[-1])
    first = ramp.color_ramp.elements[0]
    first.position, first.color = ramp_values[0]
    for position, color in ramp_values[1:]:
        element = ramp.color_ramp.elements.new(position)
        element.color = color
    for socket in list(emit.inputs["Color"].links):
        links.remove(socket)
    links.new(mix.outputs["Color"], ramp.inputs["Fac"])
    links.new(ramp.outputs["Color"], emit.inputs["Color"])
    scene.render.filepath = os.path.join(OUT, name)
    scene.view_settings.view_transform = "Standard" if colorspace == "Non-Color" else "AgX"
    scene.view_settings.look = "None" if colorspace == "Non-Color" else "AgX - Medium High Contrast"
    bpy.ops.render.render(write_still=True)

render_map("T_FC01_Surface_BaseColor_4K.png", [
    (0.00, (0.018, 0.035, 0.042, 1)),
    (0.30, (0.050, 0.082, 0.087, 1)),
    (0.52, (0.115, 0.145, 0.142, 1)),
    (0.70, (0.165, 0.188, 0.180, 1)),
    (1.00, (0.245, 0.260, 0.250, 1)),
])
render_map("T_FC01_Surface_Normal_4K.png", [
    (0.00, (0.46, 0.46, 1.0, 1)),
    (0.50, (0.50, 0.50, 1.0, 1)),
    (1.00, (0.54, 0.54, 1.0, 1)),
], "Non-Color")
# Packed ORM: R=ambient occlusion, G=roughness, B=metallic.
render_map("T_FC01_Surface_ORM_4K.png", [
    (0.00, (0.52, 0.62, 0.02, 1)),
    (0.42, (0.72, 0.78, 0.03, 1)),
    (0.72, (0.90, 0.88, 0.08, 1)),
    (1.00, (1.00, 0.94, 0.16, 1)),
], "Non-Color")

print("Generated FC01 4K baked PBR maps:", OUT)
