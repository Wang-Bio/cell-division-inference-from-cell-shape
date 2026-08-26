import pandas as pd
from pathlib import Path

INPUT_CSV = "batch_single_cell_geometry.csv"
TARGET_COLUMN = "circularity"
OUTPUT_CSV = "circularity_mean_sd_summary.csv"

df = pd.read_csv(INPUT_CSV)

if TARGET_COLUMN not in df.columns:
    raise ValueError(
        f"Column '{TARGET_COLUMN}' not found. Available columns: {list(df.columns)}"
    )

values = pd.to_numeric(df[TARGET_COLUMN], errors="coerce").dropna()

mean_value = values.mean()
sd_value = values.std(ddof=1)  # sample SD

summary = pd.DataFrame([{
    "feature": TARGET_COLUMN,
    "n": len(values),
    "mean": mean_value,
    "SD": sd_value,
    "mean_plus_minus_SD": f"{mean_value:.6f} ± {sd_value:.6f}",
}])

summary.to_csv(OUTPUT_CSV, index=False)

print(f"{TARGET_COLUMN}: {mean_value:.6f} ± {sd_value:.6f}")
print(f"Saved: {Path(OUTPUT_CSV).resolve()}")
