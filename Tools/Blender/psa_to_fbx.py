# blender -b "<Downloads>/Echidna (1)/Echidna/Echidna.blend" --python psa_to_fbx.py -- <psa> <out.fbx> [이름1,이름2,...]
# PSA 시퀀스를 Echidna.ao 아마추어에 액션으로 넣고, 아마추어+본체 메시를 FBX(액션마다 테이크)로 내보낸다.
import bpy, sys, time
from bl_ext.blender_org.io_scene_psk_psa.psa.importer import import_psa, PsaImportOptions
from psk_psa_py.psa.reader import PsaReader

argv = sys.argv[sys.argv.index('--') + 1:]
psa_path, out_path = argv[0], argv[1]
only = [x for x in argv[2].split(',') if x] if len(argv) > 2 else []

# 언리얼 스켈레탈 메시 임포트는 메시가 없으면 실패 → 본체 메시를 같이 내보내고 임포트 뒤 중복 메시만 지운다
arm = bpy.data.objects['Echidna.ao']
mesh = bpy.data.objects['Echidna.mo']
for o in list(bpy.data.objects):
    if o not in (arm, mesh):
        bpy.data.objects.remove(o, do_unlink=True)
bpy.context.view_layer.objects.active = arm
arm.select_set(True)
mesh.select_set(True)

reader = PsaReader.from_path(psa_path)
names = only or list(reader.sequences.keys())
t = time.time()
opts = PsaImportOptions(sequence_names=names, should_use_fake_user=True, should_write_metadata=False,
                        translation_scale=0.01)  # PSA 위치는 cm, 블렌더 아마추어는 m
res = import_psa(bpy.context, reader, arm, opts)
print(f'PSA2FBX imported {len(names)} actions in {time.time() - t:.1f}s, warnings {len(res.warnings)}')
for w in res.warnings[:10]:
    print('PSA2FBX warn', w)

# FBX 내보내기는 animation_data가 없는 오브젝트를 통째로 건너뛴다 — 빈 animation_data를 만들어 둔다
if not arm.animation_data:
    arm.animation_data_create()
bpy.context.scene.render.fps = 30
t = time.time()
bpy.ops.export_scene.fbx(
    filepath=out_path,
    use_selection=True,
    object_types={'ARMATURE', 'MESH'},
    add_leaf_bones=True,
    bake_anim=True,
    bake_anim_use_all_actions=True,
    bake_anim_use_nla_strips=False,
    bake_anim_force_startend_keying=True,
    bake_anim_simplify_factor=0.0,
)
print(f'PSA2FBX exported {out_path} in {time.time() - t:.1f}s')
