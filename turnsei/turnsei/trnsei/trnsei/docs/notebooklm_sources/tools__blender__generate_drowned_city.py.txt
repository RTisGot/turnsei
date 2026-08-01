import bpy
import json
import math
import os
import random
from mathutils import Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "Resource", "DrownedCity")
ASSET_DIR = os.path.join(OUT, "Assets")
EXPORT_DIR = os.path.join(OUT, "Exports")
PREVIEW_DIR = os.path.join(OUT, "Preview")
for path in (ASSET_DIR, EXPORT_DIR, PREVIEW_DIR):
    os.makedirs(path, exist_ok=True)

random.seed(217)
MATS = {}
MANIFEST = []


def clear_scene():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    for block in bpy.data.materials:
        bpy.data.materials.remove(block)
    MATS.clear()


def mat(name, color, rough=.7, metallic=0.0, emission=None, transmission=0.0, alpha=1.0):
    if name in MATS:
        return MATS[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = next((n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
    if bsdf is None:
        bsdf = m.node_tree.nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Base Color"].default_value = (*color, 1)
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["IOR"].default_value = 1.45 if transmission == 0 else 1.333
    bsdf.inputs["Transmission Weight"].default_value = transmission
    bsdf.inputs["Alpha"].default_value = alpha
    if emission:
        bsdf.inputs["Emission Color"].default_value = (*emission, 1)
        bsdf.inputs["Emission Strength"].default_value = 4.0
    if alpha < 1:
        m.surface_render_method = 'DITHERED'
    # Shared procedural surface language. Generated coordinates keep the kit
    # texture-independent while retaining consistent macro/micro scale.
    if "Glass" not in name and "Emissive" not in name and "Amber" not in name:
        nodes = m.node_tree.nodes
        links = m.node_tree.links
        tex = nodes.new("ShaderNodeTexNoise")
        tex.name = "FC01_SurfaceBreakup"
        tex.inputs["Scale"].default_value = .42 if "Water" in name else (5.5 if "Rust" in name else 2.8)
        tex.inputs["Detail"].default_value = 7.0
        tex.inputs["Roughness"].default_value = .72
        bump = nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = .18 if "Water" in name else .26
        bump.inputs["Distance"].default_value = .075 if "Water" in name else .018
        links.new(tex.outputs["Fac"], bump.inputs["Height"])
        links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])

        ramp = nodes.new("ShaderNodeValToRGB")
        dark = tuple(max(0.0, c * .55) for c in color)
        light = tuple(min(1.0, c * 1.28 + .025) for c in color)
        ramp.color_ramp.elements[0].position = .22
        ramp.color_ramp.elements[0].color = (*dark, 1)
        ramp.color_ramp.elements[1].position = .78
        ramp.color_ramp.elements[1].color = (*light, 1)
        links.new(tex.outputs["Fac"], ramp.inputs["Fac"])
        links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
    MATS[name] = m
    return m


def materials():
    return {
        "concrete": mat("M_FC01_Concrete_Dry", (0.28, .32, .33), .82),
        "wet": mat("M_FC01_Concrete_Wet", (.10, .17, .18), .25),
        "algae": mat("M_FC01_Algae_Film", (.055, .19, .13), .62),
        "salt": mat("M_FC01_Salt_Efflorescence", (.72, .71, .65), .92),
        "rust": mat("M_FC01_Rust", (.28, .075, .025), .76),
        "steel": mat("M_FC01_Steel_Galvanized", (.22, .28, .29), .38, .72),
        "glass": mat("M_FC01_Glass_Cracked", (.08, .19, .21), .13, .05, transmission=.55, alpha=.48),
        "cyan": mat("M_FC01_Tideglass_Emissive", (.015, .30, .34), .22,
                    emission=(.05, .95, .90)),
        "amber": mat("M_FC01_Amber_Remnant", (.32, .11, .025), .32,
                     emission=(1.0, .25, .035)),
        "water": mat("M_FC01_Water_Turbid", (.012, .105, .12), .07,
                     transmission=.68, alpha=.72),
        "asphalt": mat("M_FC01_Asphalt_Wet", (.045, .065, .07), .28),
    }


def tag(obj, asset, role="render"):
    obj["fc_asset"] = asset
    obj["fc_role"] = role
    return obj


def box(name, loc, scale, material, bevel=.04, asset=""):
    bpy.ops.mesh.primitive_cube_add(location=loc)
    o = bpy.context.object
    o.name = name
    o.scale = (scale[0] / 2, scale[1] / 2, scale[2] / 2)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        mod = o.modifiers.new("EdgeWear", 'BEVEL')
        mod.width = bevel
        mod.segments = 2
    o.data.materials.append(material)
    return tag(o, asset)


def cyl(name, loc, radius, depth, material, vertices=16, asset=""):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=loc)
    o = bpy.context.object
    o.name = name
    o.data.materials.append(material)
    return tag(o, asset)


def beam_between(name, a, b, width, material, asset=""):
    a, b = Vector(a), Vector(b)
    mid = (a + b) * .5
    length = (b - a).length
    o = box(name, mid, (width, width, length), material, width * .12, asset)
    o.rotation_mode = 'QUATERNION'
    o.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(b - a)
    return o


def add_waterline(asset, width, depth, z=.35, loc=(0, 0)):
    m = materials()
    box(f"{asset}_WetBand", (loc[0], loc[1], z), (width + .025, depth + .025, .72),
        m["wet"], .015, asset)
    box(f"{asset}_AlgaeBand", (loc[0], loc[1] - depth * .502, z + .05),
        (width * .88, .025, .30), m["algae"], .01, asset)
    for i in range(max(2, int(width // 1.6))):
        x = loc[0] - width * .42 + i * width * .84 / max(1, int(width // 1.6) - 1)
        box(f"{asset}_Salt_{i:02}", (x, loc[1] - depth * .505, z + .48),
            (.22 + random.random() * .20, .018, .07), m["salt"], .005, asset)


def add_rebar(asset, origin, count=5, spacing=.16, length=1.0, horizontal=True):
    m = materials()
    for i in range(count):
        if horizontal:
            o = cyl(f"{asset}_Rebar_{i:02}", (origin[0], origin[1] + i * spacing, origin[2]),
                    .018, length, m["rust"], 10, asset)
            o.rotation_euler[1] = math.radians(90)
        else:
            cyl(f"{asset}_Rebar_{i:02}", (origin[0] + i * spacing, origin[1], origin[2]),
                .018, length, m["rust"], 10, asset)


def railing(asset, length=4.0, height=.85, loc=(0, 0, 0), damaged=False):
    m = materials()
    z = loc[2] + height
    beam_between(f"{asset}_RailTop", (loc[0] - length / 2, loc[1], z),
                 (loc[0] + length / 2, loc[1], z), .045, m["steel"], asset)
    beam_between(f"{asset}_RailMid", (loc[0] - length / 2, loc[1], z - .38),
                 (loc[0] + length / 2, loc[1], z - .38), .032, m["rust"], asset)
    for i in range(int(length / .8) + 1):
        if damaged and i == int(length / .8) // 2:
            continue
        x = loc[0] - length / 2 + i * length / max(1, int(length / .8))
        post = cyl(f"{asset}_Post_{i:02}", (x, loc[1], loc[2] + height / 2),
                   .032, height, m["steel"], 10, asset)
        if damaged and i > int(length / .8) // 2:
            post.rotation_euler[1] = math.radians(5 + i * 2)


def builder_wall(asset="SM_FC01_Wall_Window_A"):
    m = materials()
    box(asset + "_L", (-1.65, 0, 1.8), (.7, .28, 3.6), m["concrete"], .05, asset)
    box(asset + "_R", (1.65, 0, 1.8), (.7, .28, 3.6), m["concrete"], .05, asset)
    box(asset + "_Top", (0, 0, 3.25), (2.6, .28, .7), m["concrete"], .05, asset)
    box(asset + "_Sill", (0, 0, .55), (2.6, .28, 1.1), m["wet"], .04, asset)
    box(asset + "_Glass", (0, .03, 2.0), (2.55, .04, 1.8), m["glass"], .01, asset)
    add_waterline(asset, 4.0, .30)


def builder_column(asset="SM_FC01_Structure_Column_Damaged_A"):
    m = materials()
    box(asset + "_Core", (0, 0, 1.9), (.74, .74, 3.8), m["concrete"], .07, asset)
    box(asset + "_Spall", (.34, -.38, .82), (.34, .08, 1.15), m["rust"], .03, asset)
    add_rebar(asset, (-.22, -.43, .82), 4, .14, 1.25, False)
    add_waterline(asset, .78, .78)


def builder_slab(asset="SM_FC01_Structure_Slab_Broken_A"):
    m = materials()
    box(asset + "_Main", (-.45, 0, .0), (3.1, 3.6, .24), m["concrete"], .05, asset)
    for i in range(6):
        beam_between(f"{asset}_Rebar_{i}", (1.0, -1.45 + i * .55, -.03),
                     (1.85 + random.random() * .35, -1.45 + i * .55, -.08),
                     .025, m["rust"], asset)
    box(asset + "_BrokenEdge", (1.25, 0, -.02), (.10, 3.2, .20), m["rust"], .01, asset)


def builder_facade(asset="SM_FC01_Building_TransitFacade_A"):
    m = materials()
    for level in range(3):
        z = level * 3.6
        for x in (-3.0, 0, 3.0):
            box(f"{asset}_Pier_{level}_{x}", (x, 0, z + 1.8), (.48, .6, 3.6),
                m["concrete"], .06, asset)
        box(f"{asset}_Slab_{level}", (0, 0, z + .18), (7.2, 3.6, .30), m["concrete"], .05, asset)
        if level > 0:
            box(f"{asset}_Glass_{level}", (0, -.31, z + 1.9), (5.5, .06, 2.25),
                m["glass"], .01, asset)
    box(asset + "_Roof", (0, 0, 10.95), (7.6, 3.9, .34), m["concrete"], .06, asset)
    railing(asset + "_RoofRail", 7.0, .85, (0, -1.72, 11.12), True)
    add_waterline(asset, 7.2, 3.6)


def builder_viaduct(asset="SM_FC01_Infrastructure_ViaductSpan_A"):
    m = materials()
    for x in (-3.6, 3.6):
        box(f"{asset}_Pier_{x}", (x, 0, 2.8), (.85, 1.15, 5.6), m["concrete"], .08, asset)
        box(f"{asset}_Cap_{x}", (x, 0, 5.45), (2.4, 1.55, .65), m["concrete"], .07, asset)
        add_waterline(asset + f"_{x}", .88, 1.18, loc=(x, 0))
    for y in (-.85, .85):
        box(f"{asset}_Girder_{y}", (0, y, 6.15), (8.8, .42, 1.15), m["concrete"], .06, asset)
    box(asset + "_Deck", (0, 0, 6.82), (9.0, 3.3, .28), m["asphalt"], .03, asset)
    railing(asset + "_Rail", 8.8, 1.0, (0, -1.55, 6.97), True)


def builder_canopy(asset="SM_FC01_Station_Canopy_A"):
    m = materials()
    box(asset + "_Platform", (0, 0, .18), (7.2, 3.0, .36), m["concrete"], .05, asset)
    for x in (-3.0, 0, 3.0):
        cyl(f"{asset}_Post_{x}", (x, 0, 1.65), .075, 3.3, m["steel"], 12, asset)
    roof = box(asset + "_Roof", (0, 0, 3.35), (7.6, 3.4, .16), m["steel"], .05, asset)
    roof.rotation_euler[1] = math.radians(-3)
    add_waterline(asset, 7.2, 3.0)


def builder_seawall(asset="SM_FC01_Civil_Seawall_A"):
    m = materials()
    box(asset + "_Caisson", (0, 0, 1.5), (6.0, 2.2, 3.0), m["concrete"], .10, asset)
    box(asset + "_Coping", (0, 0, 3.08), (6.3, 2.45, .28), m["concrete"], .06, asset)
    for x in (-2.3, 0, 2.3):
        cyl(f"{asset}_Bollard_{x}", (x, -.75, 3.48), .14, .65, m["steel"], 12, asset)
    for i in range(5):
        box(f"{asset}_Joint_{i}", (-2.4 + i * 1.2, -1.105, 1.45), (.035, .025, 2.6),
            m["salt"], .0, asset)
    add_waterline(asset, 6.0, 2.2, z=.8)


def builder_stairs(asset="SM_FC01_Civil_Stairs_A"):
    m = materials()
    steps = 12
    for i in range(steps):
        box(f"{asset}_Step_{i:02}", (0, i * .28, i * .18 + .09),
            (1.6, .34, .18), m["concrete"] if i > 3 else m["wet"], .025, asset)
    for side in (-.72, .72):
        beam_between(f"{asset}_Handrail_{side}", (side, 0, .85),
                     (side, (steps - 1) * .28, (steps - 1) * .18 + .85),
                     .045, m["steel"], asset)


def builder_skybridge(asset="SM_FC01_Infrastructure_Skybridge_A"):
    m = materials()
    box(asset + "_Floor", (0, 0, 0), (7.2, 2.2, .22), m["steel"], .04, asset)
    for side in (-1.02, 1.02):
        for i in range(5):
            x = -3.6 + i * 1.8
            beam_between(f"{asset}_Diag_{side}_{i}", (x, side, .10),
                         (x + 1.8, side, 1.8 if i % 2 == 0 else .10),
                         .075, m["rust"], asset)
        beam_between(f"{asset}_Top_{side}", (-3.6, side, 1.8), (3.6, side, 1.8),
                     .09, m["steel"], asset)
    for x in (-2.7, -.9, .9, 2.7):
        box(f"{asset}_Glass_{x}", (x, -1.04, .92), (1.55, .035, 1.35), m["glass"], .01, asset)


def builder_beacon(asset="SM_FC01_Prop_TideBeacon_A"):
    m = materials()
    cyl(asset + "_Base", (0, 0, .18), .72, .36, m["concrete"], 24, asset)
    cyl(asset + "_Mast", (0, 0, 2.0), .18, 3.7, m["rust"], 16, asset)
    for z in (.8, 1.65, 2.5):
        bpy.ops.mesh.primitive_torus_add(major_radius=.42, minor_radius=.035, location=(0, 0, z))
        bpy.context.object.data.materials.append(m["steel"])
        tag(bpy.context.object, asset)
    cyl(asset + "_Light", (0, 0, 3.95), .40, .55, m["cyan"], 24, asset)
    cyl(asset + "_Cap", (0, 0, 4.28), .52, .10, m["steel"], 24, asset)


def builder_pipe(asset="SM_FC01_Prop_DrainPipe_A"):
    m = materials()
    cyl(asset + "_Vertical", (0, 0, 1.8), .09, 3.6, m["rust"], 12, asset)
    bpy.ops.mesh.primitive_torus_add(major_radius=.28, minor_radius=.09,
                                    abso_major_rad=1.25, abso_minor_rad=.75,
                                    location=(0, .28, .12), rotation=(math.pi / 2, 0, 0))
    bpy.context.object.data.materials.append(m["rust"])
    tag(bpy.context.object, asset)
    for z in (.8, 2.4):
        bpy.ops.mesh.primitive_torus_add(major_radius=.14, minor_radius=.025, location=(0, 0, z))
        bpy.context.object.data.materials.append(m["steel"])
        tag(bpy.context.object, asset)


def builder_debris(asset="SM_FC01_Prop_DebrisCluster_A"):
    m = materials()
    for i in range(13):
        x, y = random.uniform(-1.4, 1.4), random.uniform(-1.0, 1.0)
        o = box(f"{asset}_Chunk_{i:02}", (x, y, random.uniform(.05, .22)),
                (random.uniform(.25, .75), random.uniform(.18, .55), random.uniform(.12, .38)),
                m["concrete"] if i % 3 else m["rust"], .035, asset)
        o.rotation_euler = (random.uniform(-.3, .3), random.uniform(-.3, .3), random.uniform(0, math.pi))
    add_rebar(asset, (-.5, 0, .15), 3, .18, 1.7, True)


def builder_rooftop(asset="SM_FC01_Building_RooftopShelter_A"):
    m = materials()
    box(asset + "_Base", (0, 0, .14), (5.2, 3.8, .28), m["concrete"], .05, asset)
    box(asset + "_Room", (0, 0, 1.55), (3.4, 2.6, 2.8), m["concrete"], .09, asset)
    box(asset + "_Door", (0, -1.315, 1.25), (1.0, .04, 2.05), m["steel"], .02, asset)
    box(asset + "_Awning", (0, -1.6, 2.55), (2.6, 1.0, .12), m["rust"], .04, asset)
    railing(asset + "_Rail", 4.8, .85, (0, 1.75, .28), False)
    cyl(asset + "_Tank", (1.35, .6, 3.65), .48, 1.5, m["steel"], 20, asset)
    cyl(asset + "_Lamp", (-1.25, -1.45, 2.75), .11, .32, m["amber"], 16, asset)


def builder_water(asset="SM_FC01_Water_TurbidPlane_A"):
    m = materials()
    box(asset + "_Surface", (0, 0, 0), (12, 12, .06), m["water"], .01, asset)
    for i in range(9):
        bpy.ops.mesh.primitive_torus_add(major_radius=.35 + i * .18, minor_radius=.008,
                                        location=(random.uniform(-3, 3), random.uniform(-3, 3), .04))
        bpy.context.object.data.materials.append(m["cyan"])
        tag(bpy.context.object, asset)


def builder_railing(asset="SM_FC01_Prop_Railing_Damaged_A"):
    materials()
    box(asset + "_Footing", (0, 0, .06), (4.2, .35, .12), materials()["concrete"], .02, asset)
    railing(asset, 4.0, .85, (0, 0, .12), True)


def builder_bollard(asset="SM_FC01_Prop_MooringBollard_A"):
    m = materials()
    cyl(asset + "_Foot", (0, 0, .08), .38, .16, m["concrete"], 16, asset)
    cyl(asset + "_Stem", (0, 0, .38), .15, .62, m["rust"], 16, asset)
    beam_between(asset + "_Horn", (-.34, 0, .65), (.34, 0, .65), .15, m["steel"], asset)


def builder_ladder(asset="SM_FC01_Prop_SeawallLadder_A"):
    m = materials()
    for x in (-.28, .28):
        cyl(f"{asset}_Side_{x}", (x, 0, 1.5), .032, 3.0, m["rust"], 10, asset)
    for i in range(9):
        beam_between(f"{asset}_Rung_{i}", (-.28, 0, .25 + i * .32),
                     (.28, 0, .25 + i * .32), .028, m["steel"], asset)


def builder_storefront(asset="SM_FC01_Building_Storefront_Broken_A"):
    m = materials()
    box(asset + "_Frame", (0, 0, 1.6), (4.8, .42, 3.2), m["concrete"], .06, asset)
    box(asset + "_Void", (0, -.24, 1.55), (3.7, .05, 2.35), m["glass"], .01, asset)
    for x in (-1.75, 0, 1.75):
        box(f"{asset}_Mullion_{x}", (x, -.29, 1.6), (.10, .10, 2.5), m["rust"], .02, asset)
    box(asset + "_Awning", (0, -.72, 2.95), (4.5, 1.15, .14), m["steel"], .04, asset)
    add_waterline(asset, 4.8, .44)


def builder_cabinet(asset="SM_FC01_Prop_UtilityCabinet_A"):
    m = materials()
    box(asset + "_Body", (0, 0, .85), (1.05, .55, 1.7), m["steel"], .06, asset)
    box(asset + "_Door", (0, -.29, .85), (.88, .035, 1.5), m["rust"], .02, asset)
    box(asset + "_Handle", (.29, -.33, .88), (.06, .05, .28), m["steel"], .01, asset)
    add_waterline(asset, 1.05, .55, z=.30)


def builder_sign(asset="SM_FC01_Prop_WayfindingFrame_A"):
    m = materials()
    for x in (-.85, .85):
        cyl(f"{asset}_Post_{x}", (x, 0, 1.25), .045, 2.5, m["rust"], 10, asset)
    box(asset + "_Panel", (0, 0, 2.0), (2.0, .09, .72), m["steel"], .04, asset)
    box(asset + "_GlowLine", (0, -.055, 2.0), (1.52, .025, .055), m["cyan"], .01, asset)


def builder_aircon(asset="SM_FC01_Prop_HVAC_Rooftop_A"):
    m = materials()
    box(asset + "_Body", (0, 0, .65), (1.6, 1.1, 1.3), m["steel"], .08, asset)
    cyl(asset + "_Fan", (0, -.57, .72), .39, .06, m["rust"], 20, asset).rotation_euler[0] = math.radians(90)
    for x in (-.42, 0, .42):
        box(f"{asset}_Grille_{x}", (x, -.61, .72), (.025, .02, .7), m["steel"], 0, asset)
    for x in (-.58, .58):
        box(f"{asset}_Foot_{x}", (x, 0, .08), (.18, .8, .16), m["concrete"], .02, asset)


def builder_floating_debris(asset="SM_FC01_Prop_FloatingDebris_A"):
    m = materials()
    for i in range(9):
        x, y = random.uniform(-1.8, 1.8), random.uniform(-.8, .8)
        o = box(f"{asset}_Piece_{i:02}", (x, y, random.uniform(.02, .07)),
                (random.uniform(.25, .85), random.uniform(.08, .28), random.uniform(.04, .11)),
                m["rust"] if i % 3 == 0 else m["concrete"], .02, asset)
        o.rotation_euler.z = random.uniform(0, math.pi)


def builder_vegetation(asset="SM_FC01_Vegetation_ReedCards_A"):
    m = materials()
    leaf = mat("M_FC01_VegetationCard", (.08, .24, .13), .82, alpha=.92)
    for i in range(18):
        angle = random.uniform(0, math.pi * 2)
        radius = random.uniform(.05, .65)
        x, y = math.cos(angle) * radius, math.sin(angle) * radius
        h = random.uniform(.55, 1.35)
        card = box(f"{asset}_Card_{i:02}", (x, y, h * .5), (.025, .16, h),
                   leaf, .008, asset)
        card.rotation_euler.z = angle
        card.rotation_euler.x = random.uniform(-.10, .10)
    box(asset + "_RootMat", (0, 0, .025), (1.5, 1.5, .05), m["algae"], .03, asset)


BUILDERS = [
    builder_wall, builder_column, builder_slab, builder_facade, builder_viaduct,
    builder_canopy, builder_seawall, builder_stairs, builder_skybridge,
    builder_beacon, builder_pipe, builder_debris, builder_rooftop, builder_water,
    builder_railing, builder_bollard, builder_ladder, builder_storefront,
    builder_cabinet, builder_sign, builder_aircon, builder_floating_debris,
    builder_vegetation,
]


def asset_name(builder):
    return builder.__defaults__[0]


def add_lod_and_collision(asset):
    render_objs = [o for o in bpy.context.scene.objects if o.type == 'MESH' and o.get("fc_role") == "render"]
    if not render_objs:
        return
    # Conservative proxy collision; excluded from render but preserved in .blend.
    mins = Vector((1e9, 1e9, 1e9))
    maxs = Vector((-1e9, -1e9, -1e9))
    for o in render_objs:
        for c in o.bound_box:
            p = o.matrix_world @ Vector(c)
            mins.x, mins.y, mins.z = min(mins.x, p.x), min(mins.y, p.y), min(mins.z, p.z)
            maxs.x, maxs.y, maxs.z = max(maxs.x, p.x), max(maxs.y, p.y), max(maxs.z, p.z)
    size, center = maxs - mins, (maxs + mins) * .5
    proxy = box("UCX_" + asset + "_01", center, size, materials()["concrete"], 0, asset)
    proxy["fc_role"] = "collision"
    proxy.display_type = 'WIRE'
    proxy.hide_render = True
    # Keep two joined decimated representations inside each editable .blend.
    for level, ratio in ((1, .52), (2, .22)):
        bpy.ops.object.select_all(action='DESELECT')
        copies = []
        for src in render_objs:
            dup = src.copy()
            dup.data = src.data.copy()
            bpy.context.collection.objects.link(dup)
            dup.select_set(True)
            copies.append(dup)
        if not copies:
            continue
        bpy.context.view_layer.objects.active = copies[0]
        bpy.ops.object.join()
        lod = bpy.context.object
        lod.name = f"{asset}_LOD{level}"
        lod["fc_asset"] = asset
        lod["fc_role"] = f"lod{level}"
        dec = lod.modifiers.new(f"LOD{level}_Decimate", 'DECIMATE')
        dec.ratio = ratio
        bpy.context.view_layer.objects.active = lod
        bpy.ops.object.modifier_apply(modifier=dec.name)
        lod.hide_render = True
        lod.hide_set(True)


def export_current(asset):
    # Every source mesh receives a non-overlapping local UV unwrap suitable for
    # the shared 4K material atlas and later per-asset rebakes.
    for o in [x for x in bpy.context.scene.objects if x.type == 'MESH' and x.get("fc_role") == "render"]:
        bpy.ops.object.select_all(action='DESELECT')
        o.hide_set(False)
        o.select_set(True)
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=.025)
        bpy.ops.object.mode_set(mode='OBJECT')
    add_lod_and_collision(asset)
    bpy.context.scene["fc_unit"] = "meters"
    bpy.context.scene["fc_waterline_m"] = 0.70
    bpy.context.scene.unit_settings.system = 'METRIC'
    bpy.context.scene.unit_settings.scale_length = 1.0
    blend_path = os.path.join(ASSET_DIR, asset + ".blend")
    bpy.ops.wm.save_as_mainfile(filepath=blend_path)

    for o in bpy.context.scene.objects:
        o.select_set(o.type == 'MESH' and o.get("fc_role") == "render")
    bpy.ops.export_scene.fbx(
        filepath=os.path.join(EXPORT_DIR, asset + ".fbx"),
        use_selection=True, apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL',
        axis_forward='-Z', axis_up='Y', add_leaf_bones=False, bake_anim=False,
        path_mode='AUTO')
    bpy.ops.export_scene.gltf(
        filepath=os.path.join(EXPORT_DIR, asset + ".glb"),
        export_format='GLB', use_selection=True, export_apply=True)

    dims = []
    for o in bpy.context.scene.objects:
        if o.type == 'MESH' and o.get("fc_role") == "render":
            dims.append([round(v, 3) for v in o.dimensions])
    MANIFEST.append({
        "name": asset,
        "blend": os.path.relpath(blend_path, ROOT).replace("\\", "/"),
        "fbx": f"Resource/DrownedCity/Exports/{asset}.fbx",
        "glb": f"Resource/DrownedCity/Exports/{asset}.glb",
        "unit": "meter",
        "origin": "ground_center",
        "waterline_m": 0.70,
        "mesh_parts": len(dims),
    })


def add_builder_at(builder, loc, rot=0.0):
    before = set(bpy.context.scene.objects)
    builder()
    for o in set(bpy.context.scene.objects) - before:
        o.location += Vector(loc)
        o.rotation_euler.z += rot


def build_master():
    clear_scene()
    m = materials()
    # Connected district: two raised banks around a flooded transit corridor.
    box("District_LeftBank", (-11.5, 2, .22), (19, 30, .72), m["asphalt"], .08, "ENV_FC01_District")
    box("District_RightBank", (11.5, 2, .22), (13, 30, .72), m["asphalt"], .08, "ENV_FC01_District")
    box("District_CanalWater", (0, 1, .58), (9.5, 34, .09), m["water"], .01, "ENV_FC01_District")
    add_builder_at(builder_facade, (-10, 8, .62), math.radians(-3))
    add_builder_at(builder_facade, (10, 10, .62), math.radians(180))
    add_builder_at(builder_rooftop, (10, -5, .62), math.radians(180))
    add_builder_at(builder_viaduct, (0, 11, .58), 0)
    add_builder_at(builder_seawall, (-5.0, -3, .58), math.radians(90))
    add_builder_at(builder_seawall, (5.0, 3, .58), math.radians(-90))
    add_builder_at(builder_stairs, (7.0, -5, .63), math.radians(180))
    add_builder_at(builder_skybridge, (0, 7, 7.9), 0)
    add_builder_at(builder_canopy, (-10, -4, .62), math.radians(2))
    add_builder_at(builder_beacon, (8.5, 3, .62), 0)
    add_builder_at(builder_wall, (-7, -10, .62), math.radians(90))
    add_builder_at(builder_column, (-5.4, 1, .62), 0)
    add_builder_at(builder_slab, (4, -1, .9), math.radians(12))
    add_builder_at(builder_pipe, (-13, 8, .75), 0)
    add_builder_at(builder_debris, (-2, -8, .66), 0)
    add_builder_at(builder_water, (0, -8, .60), 0)

    # Environment lighting and camera.
    bpy.ops.object.light_add(type='SUN', location=(8, -12, 18))
    sun = bpy.context.object
    sun.name = "Key_OvercastSun"
    sun.data.energy = 2.2
    sun.rotation_euler = (math.radians(28), math.radians(-18), math.radians(28))
    bpy.ops.object.light_add(type='AREA', location=(-5, -7, 16))
    bpy.context.object.data.energy = 1300
    bpy.context.object.data.shape = 'DISK'
    bpy.context.object.data.size = 14
    bpy.ops.object.camera_add(location=(25, -31, 14))
    cam = bpy.context.object
    cam.name = "CAM_FC01_Hero"
    direction = Vector((0, 2, 4.0)) - cam.location
    cam.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()
    cam.data.lens = 43
    bpy.context.scene.camera = cam

    world = bpy.context.scene.world or bpy.data.worlds.new("World")
    bpy.context.scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (.025, .055, .07, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = .42

    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_EEVEE_NEXT'
    scene.render.resolution_x = 1280
    scene.render.resolution_y = 720
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.filepath = os.path.join(PREVIEW_DIR, "ENV_FC01_DrownedTransitDistrict.png")
    scene.render.film_transparent = False
    scene.view_settings.look = 'AgX - Medium High Contrast'
    scene.render.image_settings.color_mode = 'RGBA'
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "ENV_FC01_DrownedTransitDistrict.blend"))
    bpy.ops.object.select_all(action='DESELECT')
    for o in scene.objects:
        if o.type == 'MESH':
            o.select_set(True)
    bpy.ops.export_scene.gltf(
        filepath=os.path.join(EXPORT_DIR, "ENV_FC01_DrownedTransitDistrict.glb"),
        export_format='GLB', use_selection=True, export_apply=True)
    bpy.ops.export_scene.fbx(
        filepath=os.path.join(EXPORT_DIR, "ENV_FC01_DrownedTransitDistrict.fbx"),
        use_selection=True, apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL',
        axis_forward='-Z', axis_up='Y', add_leaf_bones=False, bake_anim=False)
    bpy.ops.render.render(write_still=True)


def main():
    for builder in BUILDERS:
        clear_scene()
        asset = asset_name(builder)
        builder()
        export_current(asset)
    build_master()
    with open(os.path.join(OUT, "asset_manifest.json"), "w", encoding="utf-8") as f:
        json.dump({"kit": "TIDEGLASS_FC01", "assets": MANIFEST}, f, ensure_ascii=False, indent=2)
    print(f"Generated {len(MANIFEST)} assets in {OUT}")


if __name__ == "__main__":
    main()
