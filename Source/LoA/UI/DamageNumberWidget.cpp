#include "DamageNumberWidget.h"
#include "Components/TextBlock.h"

void UDamageNumberWidget::SetDamage(float Damage)
{
	if (!DamageText) return;

	// 데미지가 수만 단위라 자릿수 구분이 없으면 읽기 어렵다 (보스 HP 표기와 같은 방침)
	DamageText->SetText(FText::FromString(FString::FormatAsNumber(FMath::RoundToInt(Damage))));
}

void UDamageNumberWidget::SetLabel(const FText& Label, FLinearColor Color, float FontSizeScale)
{
	if (!DamageText) return;

	DamageText->SetText(Label);
	DamageText->SetColorAndOpacity(FSlateColor(Color));

	// WBP에서 잡은 폰트(패밀리·아웃라인)는 유지하고 크기만 배율로 키운다
	if (!FMath::IsNearlyEqual(FontSizeScale, 1.f))
	{
		FSlateFontInfo Font = DamageText->GetFont();
		Font.Size = FMath::Max(1.f, Font.Size * FontSizeScale);
		DamageText->SetFont(Font);
	}
}
