# blender -b --python psk_to_static_fbx.py -- <psk> <out.fbx> [슬롯이름1,슬롯이름2,...]
# 스키닝된 PSK(소품 — 거울 등)를 아마추어를 떼고 정적 메시 FBX로 내보낸다. 텍스처는 넣지 않음(언리얼에서 MI로 연결).
# 함정: PSK 임포터의 0.01 스케일은 아마추어에 걸려 있어서 부모만 끊으면 메시가 100배가 됨 → 월드 행렬을 유지한 채 끊고 transform_apply
# 슬롯 이름을 주면 머티리얼 슬롯 순서대로 이름을 바꾼다(언리얼 슬롯 이름이 됨)
import bpy, sys
from bl_ext.blender_org.io_scene_psk_psa.psk.importer import import_psk, PskImportOptions
from psk_psa_py.psk.reader import read_psk_from_file

args = sys.argv[sys.argv.index('--') + 1:]
psk, out = args[0], args[1]
slot_names = args[2].split(',') if len(args) > 2 else []

bpy.ops.wm.read_homefile(use_empty=True)  # factory 리셋은 애드온 등록까지 지움
opts = PskImportOptions()
opts.scale = 0.01
import_psk(read_psk_from_file(psk), bpy.context, 'StaticProp', opts)

ob = next(x for x in bpy.data.objects if x.type == 'MESH')
for mod in list(ob.modifiers):
    ob.modifiers.remove(mod)
mw = ob.matrix_world.copy()
ob.parent = None
ob.matrix_world = mw
for x in list(bpy.data.objects):
    if x.type == 'ARMATURE':
        bpy.data.objects.remove(x)
ob.vertex_groups.clear()

bpy.ops.object.select_all(action='DESELECT')
ob.select_set(True)
bpy.context.view_layer.objects.active = ob
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

for m, n in zip(ob.data.materials, slot_names):
    m.name = n

bpy.ops.export_scene.fbx(filepath=out, use_selection=True, object_types={'MESH'}, bake_anim=False,
                         path_mode='STRIP', mesh_smooth_type='FACE')
zs = [v.co.z for v in ob.data.vertices]
print(f'PSK2STATIC ok {out} slots {[m.name for m in ob.data.materials]} height {max(zs) - min(zs):.3f}m')
