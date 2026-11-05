#pragma once

#include "CoreMinimal.h"

class AActor;
class UAudioComponent;
class USoundBase;

/** 원본 소리 하나 또는 번호 범위(무작위) — KR/WwiseDefaultBank_S_Mob_G_Echidna<Bank>의 <First>~<Last>. Bank 0 = 소리 없음, 101~ = BigEchidna<Bank-100>.
 *  Voice > 0이면 같은 Voice 소리는 한 번에 하나만 — 새로 나면 앞 소리를 끊고, 같은 소리가 0.2초 안에 또 불리면 건너뛴다(연꽃처럼 여러 개가 시간차로 터지는 것) */
struct FRaidCue
{
	int32 Bank = 0;
	int32 First = 0;
	int32 Last = 0;
	int32 Voice = 0;

	constexpr FRaidCue() = default;
	constexpr FRaidCue(int32 InBank, int32 Number) : Bank(InBank), First(Number), Last(Number) {}
	constexpr FRaidCue(int32 InBank, int32 InFirst, int32 InLast) : Bank(InBank), First(InFirst), Last(InLast) {}
	constexpr FRaidCue(int32 InBank, int32 InFirst, int32 InLast, int32 InVoice) : Bank(InBank), First(InFirst), Last(InLast), Voice(InVoice) {}
};

/** 패턴 ↔ 소리 번호 표(유저가 듣고 정함) — 소리를 바꿀 때는 여기만 고치면 된다 */
namespace RaidCue
{
	// 카운터
	inline constexpr FRaidCue CounterSplit(1, 87);			// 4갈래로 갈라질 때
	inline constexpr FRaidCue CounterReturn(1, 88);			// 한 방향에서 돌아올 때
	// 정면 리본
	inline constexpr FRaidCue RibbonThrow(1, 39);			// 리본 뻗을 때
	inline constexpr FRaidCue RibbonLeap(1, 42);			// 1차 빗나가 그 방향으로 날아갈 때
	inline constexpr FRaidCue RibbonArc(1, 41);				// 2차도 빗나가 앞에 내리칠 때
	// 끌고 간 후 장판
	inline constexpr FRaidCue DragFanTether(1, 409);		// 리본 뻗을 때
	inline constexpr FRaidCue DragFanFirst(1, 367);			// 1번 부채꼴
	inline constexpr FRaidCue DragFanSecond(1, 368);		// 2번 부채꼴
	// 두번 긋고 도넛
	inline constexpr FRaidCue DonutSlash1(1, 27);			// 첫 휘두름
	inline constexpr FRaidCue DonutSlash2(1, 28);			// 다음 휘두름
	inline constexpr FRaidCue DonutSmall(1, 29);			// 작은 도넛 폭발
	inline constexpr FRaidCue DonutRise(2, 22);				// 올라갈 때
	inline constexpr FRaidCue DonutFall(2, 23);				// 내려올 때
	inline constexpr FRaidCue DonutOuter(1, 30);			// 마지막 바깥 장판 폭발
	// 뒤로 빠지며 연꽃
	inline constexpr FRaidCue RetreatLotusJump(1, 149);		// 뒤로 점프
	inline constexpr FRaidCue RetreatLotusFan(1, 151);		// 전방 부채꼴
	// 연꽃(뒤로 빠지며 연꽃·랜잡 공용)
	inline constexpr FRaidCue LotusBloom(1, 182, 182, 1);	// 필 때 — 여러 개가 시간차로 피고 터져도 겹치지 않게 Voice 1 공유
	inline constexpr FRaidCue LotusBurst(1, 189, 189, 1);	// 터질 때
	// 똥장판
	inline constexpr FRaidCue PoopStart(1, 691);			// 패턴 시작
	inline constexpr FRaidCue PoopSegment(1, 5, 9);			// 직사각형 칸이 순차 폭발할 때마다
	inline constexpr FRaidCue PoopCircle(1, 10);			// 원형 폭발
	// 하트
	inline constexpr FRaidCue HeartFire(1, 475);			// 하트 발사(백스텝·전방향 공용)
	inline constexpr FRaidCue BackstepHeartFan(1, 476);		// 백스텝 하트 뒤 부채꼴 폭발
	// 뒤로 빠지며 좌우장판
	inline constexpr FRaidCue RetreatFanBurst(1, 10, 13);	// 장판 폭발
	// 육각 리본
	inline constexpr FRaidCue HexVertical(1, 10);			// 세로
	inline constexpr FRaidCue HexHorizontal(1, 27);			// 가로
	inline constexpr FRaidCue HexEdge(1, 10, 13);			// 외곽 육각형 변마다
	// 장풍
	inline constexpr FRaidCue PalmBlast(1, 485);			// 발사
	// 되돌아오는 구체
	inline constexpr FRaidCue OrbThrow(1, 295);				// 1·2번째
	inline constexpr FRaidCue OrbFan(1, 316);				// 마지막 4개
	// 큰 패턴
	inline constexpr FRaidCue MirrorLink(1, 627);			// 거울잇기 시작
	inline constexpr FRaidCue Swing(1, 673);				// 그네 시작
	inline constexpr FRaidCue RandomGrabLine(2, 262);		// 랜잡 대사
	inline constexpr FRaidCue MirrorCounterIntro(2, 58);	// 거울 카운터 컷신 시작
	inline constexpr FRaidCue SkyMirrorFollow(1, 502);		// 8거울 때 플레이어를 따라오는 거울(반복)
	inline constexpr FRaidCue Groggy(1, 371);				// 보스 그로기 진입
	// 0줄 발악 — 거울 섞기
	inline constexpr FRaidCue FinalMirrorAppear(2, 132);	// 거울 등장
	inline constexpr FRaidCue FinalRedGlow(3, 332);			// 빨간 거울 발광
	inline constexpr FRaidCue FinalPortraitStart(1, 89);	// 빨간 거울이 꺼지고 초상화가 지나가기 전 한 번
	inline constexpr FRaidCue FinalSwap(1, 93);				// 거울 스왑(12초짜리 — 다음 스왑·카운터 대기에서 끊음)
	inline constexpr FRaidCue FinalCounterWait(1, 85);		// 카운터 대기 동안 반복(2.7초짜리)
	// 0줄 발악 — 격돌
	inline constexpr FRaidCue ClashGrab(1, 268);			// 격돌을 잡을 때
	inline constexpr FRaidCue ClashLoop(1, 396);			// 격돌 진행 중(29초짜리 — 결과가 나면 끊음)
	inline constexpr FRaidCue ClashNoteHit(101, 126);		// 타이밍 성공(BigEchidna1)
	inline constexpr FRaidCue ClashBad(1, 201);				// MISS·실패
	inline constexpr FRaidCue ClashMash(1, 167);			// 마지막 연타(결과가 나면 끊음)
	inline constexpr FRaidCue ClashWin(1, 168);				// 연타 성공
	inline constexpr FRaidCue ClashBossDeath(1, 597);		// 격돌 성공 후 보스 쓰러짐
	// 플레이어
	inline constexpr FRaidCue PlayerCharm(2, 74);			// 매혹 스택이 쌓일 때

	// 레이드 진입 때 미리 읽을 목록 — 위에 소리를 추가하면 여기에도 넣을 것
	inline constexpr FRaidCue All[] = {
		CounterSplit, CounterReturn, RibbonThrow, RibbonLeap, RibbonArc, DragFanTether, DragFanFirst, DragFanSecond,
		DonutSlash1, DonutSlash2, DonutSmall, DonutRise, DonutFall, DonutOuter, RetreatLotusJump, RetreatLotusFan,
		LotusBloom, LotusBurst, PoopStart, PoopSegment, PoopCircle, HeartFire, BackstepHeartFan, RetreatFanBurst,
		HexVertical, HexHorizontal, HexEdge, PalmBlast, OrbThrow, OrbFan, MirrorLink, Swing,
		RandomGrabLine, MirrorCounterIntro, SkyMirrorFollow, Groggy, FinalMirrorAppear, FinalRedGlow, FinalPortraitStart, FinalSwap,
		FinalCounterWait, ClashGrab, ClashLoop, ClashNoteHit, ClashBad, ClashMash, ClashWin, ClashBossDeath,
		PlayerCharm,
	};
}

/** 에키드나 레이드 원본 사운드(/Game/LostArk/Raid/EchidnaModling/sound/<Folder>/<Name>) 재생 도우미.
 *  볼륨 = 게임 설정 효과음 × 배율(Play는 `LoA.RaidSound.PatternGain`, 나머지는 `LoA.RaidSound.Gain`). 같은 소리가 같은 프레임에 여러 번 불리면(4거울 4개가 동시에 나옴 등) 한 번만 — 겹치면 크고 뭉개짐.
 *  어느 패턴에 어떤 소리인지는 Raid/CLAUDE.md "레이드 사운드" */
namespace LoARaidSound
{
	LOA_API USoundBase* Load(const TCHAR* Folder, const TCHAR* Name);
	/** `LoA.Raid.Preload` — 끄면 예전처럼 처음 쓸 때 읽는다(성능 비교용) */
	LOA_API bool IsPreloadEnabled();
	/** RaidCue::All과 거울 소리를 전부 읽어 Out에 담는다 — 받는 쪽이 Out을 UPROPERTY로 들고 있어야 GC되지 않는다 */
	LOA_API void PreloadAll(TArray<TObjectPtr<UObject>>& Out);
	/** 패턴 소리 — 액터에 붙지 않아 부른 쪽(장판·리본)이 먼저 사라져도 끝까지 나온다. 범위면 무작위 하나 */
	LOA_API void Play(const UObject* WorldContext, const FRaidCue& Cue);
	/** Play와 같은데 끊거나 반복할 수 있게 컴포넌트를 돌려준다. bAutoDestroy=false면 끝나도 남아 KeepLooping으로 다시 튼다 — 반드시 Stop.
	 *  같은 프레임 중복이거나 효과음 0이면 nullptr */
	LOA_API UAudioComponent* Spawn(const UObject* WorldContext, const FRaidCue& Cue, bool bAutoDestroy = true);
	/** 액터에 붙여 재생 — 반환 컴포넌트로 끊거나 다시 틀 수 있다. bAutoDestroy=false면 끝나도 남아 다시 Play 가능(반복용).
	 *  같은 프레임 중복이거나 효과음 0이면 nullptr */
	LOA_API UAudioComponent* PlayAttached(AActor* Owner, const TCHAR* Folder, const TCHAR* Name, bool bAutoDestroy = true);
	LOA_API void PlayAt(const UObject* WorldContext, const TCHAR* Folder, const TCHAR* Name, const FVector& Location);
	/** 반복용 컴포넌트가 끝났으면 처음부터 다시 */
	LOA_API void KeepLooping(UAudioComponent* Comp);
	/** 끊고 정리까지(bAutoDestroy=false로 만든 컴포넌트도 사라진다) */
	LOA_API void Stop(UAudioComponent* Comp, float FadeOut = 0.2f);
}
