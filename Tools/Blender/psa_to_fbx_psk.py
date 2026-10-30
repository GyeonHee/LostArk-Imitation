# blender -b --python psa_to_fbx_psk.py -- <base.psk> <armature_name> <psa> <out.fbx> [이름1,이름2,...]
# 기준 PSK(메시+아마추어)를 0.01배로 들여와 PSA 시퀀스를 액션으로 넣고, FBX(액션마다 테이크)로 내보낸다.
# psa_to_fbx.py(에키드나 — .blend 기준)와 같은 함정 처리: PSA 위치 cm → translation_scale 0.01,
# animation_data 없으면 FBX가 액션을 빠뜨림, 메시 없는 FBX는 MCP 스켈레탈 임포트 실패(메시 포함, 언리얼에서 중복 메시 삭제)
import bpy, sys, time
from bl_ext.blender_org.io_scene_psk_psa.psk.importer import import_psk, PskImportOptions
from bl_ext.blender_org.io_scene_psk_psa.psa.importer import import_psa, PsaImportOptions
from psk_psa_py.psk.reader import read_psk_from_file
from psk_psa_py.psa.reader import PsaReader

argv = sys.argv[sys.argv.index('--') + 1:]
base_psk, arm_name, psa_path, out_path = argv[:4]
only = [x for x in argv[4].split(',') if x] if len(argv) > 4 else []

bpy.ops.wm.read_homefile(use_empty=True)  # factory 리셋은 애드온 등록까지 지움
opts = PskImportOptions()
opts.scale = 0.01
opts.should_import_materials = False
import_psk(read_psk_from_file(base_psk), bpy.context, 'base', opts)
arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
arm.name = arm_name
bpy.context.view_layer.objects.active = arm
for o in bpy.data.objects:
    o.select_set(True)

reader = PsaReader.from_path(psa_path)
names = only or list(reader.sequences.keys())
t = time.time()
res = import_psa(bpy.context, reader, arm, PsaImportOptions(sequence_names=names, should_use_fake_user=True,
                                                           should_write_metadata=False, translation_scale=0.01))
print(f'PSA2FBX imported {len(names)} actions in {time.time() - t:.1f}s, warnings {len(res.warnings)}')
for w in res.warnings[:5]:
    print('PSA2FBX warn', w[:200])

if not arm.animation_data:
    arm.animation_data_create()
bpy.context.scene.render.fps = 30
t = time.time()
bpy.ops.export_scene.fbx(filepath=out_path, use_selection=True, object_types={'ARMATURE', 'MESH'},
                         add_leaf_bones=True, bake_anim=True, bake_anim_use_all_actions=True,
                         bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True,
                         bake_anim_simplify_factor=0.0)
print(f'PSA2FBX exported {out_path} in {time.time() - t:.1f}s')
