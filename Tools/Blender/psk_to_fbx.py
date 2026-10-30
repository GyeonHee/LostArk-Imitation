# blender -b --python psk_to_fbx.py -- <jobs.json>
# jobs.json = [{"psk": 경로, "out": FBX 경로, "tex_dirs": [텍스처 폴더...], "mat_dir": .mat 폴더, "armature_name": 선택}]
# PSK(cm)를 0.01배로 들여와 .mat(umodel 메모)대로 Diffuse/Normal 텍스처를 연결하고, 텍스처를 넣은 FBX로 내보낸다.
# armature_name을 주면 아마추어 오브젝트 이름을 바꾼다 — FBX 루트 본 이름이 같아야 언리얼에서 기존 스켈레톤에 붙는다.
import bpy, sys, os, json, time
from bl_ext.blender_org.io_scene_psk_psa.psk.importer import import_psk, PskImportOptions
from psk_psa_py.psk.reader import read_psk_from_file


def read_mat(path):
    out = {}
    if os.path.exists(path):
        for line in open(path, encoding='utf-8', errors='ignore'):
            if '=' in line:
                k, v = line.strip().split('=', 1)
                out[k] = v
    return out


def find_tex(name, dirs):
    for d in dirs:
        for ext in ('.tga', '.png', '.dds'):
            p = os.path.join(d, name + ext)
            if os.path.exists(p):
                return p
    return None


def wire_material(mat, info, dirs):
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = next((n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED'), None)
    if not bsdf:
        return 0
    wired = 0
    diffuse = find_tex(info.get('Diffuse', ''), dirs)
    if diffuse:
        tex = nt.nodes.new('ShaderNodeTexImage')
        tex.image = bpy.data.images.load(diffuse, check_existing=True)
        nt.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
        wired += 1
    normal = find_tex(info.get('Normal', ''), dirs)
    if normal:
        tex = nt.nodes.new('ShaderNodeTexImage')
        tex.image = bpy.data.images.load(normal, check_existing=True)
        tex.image.colorspace_settings.name = 'Non-Color'
        nm = nt.nodes.new('ShaderNodeNormalMap')
        nt.links.new(tex.outputs['Color'], nm.inputs['Color'])
        nt.links.new(nm.outputs['Normal'], bsdf.inputs['Normal'])
        wired += 1
    return wired


def run_job(job):
    bpy.ops.wm.read_homefile(use_empty=True)  # factory 리셋은 애드온 등록까지 지움
    opts = PskImportOptions()
    opts.scale = 0.01
    name = os.path.splitext(os.path.basename(job['psk']))[0]
    res = import_psk(read_psk_from_file(job['psk']), bpy.context, name, opts)
    arm = next((o for o in bpy.data.objects if o.type == 'ARMATURE'), None)
    if arm and job.get('armature_name'):
        arm.name = job['armature_name']
    wired = 0
    for mat in bpy.data.materials:
        wired += wire_material(mat, read_mat(os.path.join(job.get('mat_dir', ''), mat.name + '.mat')), job.get('tex_dirs', []))
    for o in bpy.data.objects:
        o.select_set(True)
    os.makedirs(os.path.dirname(job['out']), exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=job['out'], use_selection=True, object_types={'ARMATURE', 'MESH'},
                             add_leaf_bones=True, bake_anim=False, path_mode='COPY', embed_textures=True)
    print(f'PSK2FBX ok {name} mats {len(bpy.data.materials)} wired {wired} warn {len(res.warnings)} -> {job["out"]}')


jobs = json.load(open(sys.argv[sys.argv.index('--') + 1], encoding='utf-8'))
t = time.time()
for job in jobs:
    try:
        run_job(job)
    except Exception as e:
        print(f'PSK2FBX FAIL {job["psk"]}: {e}')
print(f'PSK2FBX done {len(jobs)} in {time.time() - t:.0f}s')
