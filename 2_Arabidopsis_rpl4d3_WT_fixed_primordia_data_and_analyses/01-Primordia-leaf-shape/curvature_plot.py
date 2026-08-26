import glob
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt


def resample_contour_equal_arc_length(x, y, n_points=100, closed=False):
    x = np.asarray(x, dtype=float).ravel()
    y = np.asarray(y, dtype=float).ravel()
    if x.size != y.size or x.size < 2:
        raise ValueError("x and y must have the same length and contain at least 2 points.")

    dx = np.diff(x)
    dy = np.diff(y)
    seglen = np.hypot(dx, dy)

    keep = seglen > 1e-12
    if not np.all(keep):
        idx = np.concatenate([np.where(keep)[0], [len(seglen)]])
        x = x[idx]
        y = y[idx]
        dx = np.diff(x)
        dy = np.diff(y)
        seglen = np.hypot(dx, dy)

    if seglen.size == 0 or np.all(seglen <= 0):
        raise ValueError("Contour has zero total length.")

    s = np.concatenate([[0.0], np.cumsum(seglen)])
    total = s[-1]
    s_target = np.linspace(0.0, total, n_points)

    xr = np.interp(s_target, s, x)
    yr = np.interp(s_target, s, y)
    return xr, yr


def load_process_parameterize(csv_path, n_points=100):
    df = pd.read_csv(csv_path, header=None, names=["id", "x", "y"], skipinitialspace=True)
    df["x"] = pd.to_numeric(df["x"], errors="coerce")
    df["y"] = pd.to_numeric(df["y"], errors="coerce")
    df = df.dropna(subset=["x", "y"])

    y_min = df["y"].min()
    y_max = df["y"].max()
    y_range = y_max - y_min
    if y_range == 0:
        raise ValueError(f"{csv_path}: y_range is zero.")

    x_mean = df["x"].mean()

    x_norm = (df["x"] - x_mean) / y_range
    y_norm = (df["y"] - y_min) / y_range

    # flip y (upside down)
    y_flip = 1.0 - y_norm

    # parameterize to N points (OPEN)
    x_p, y_p = resample_contour_equal_arc_length(x_norm, y_flip, n_points=n_points, closed=False)
    return x_p, y_p


def curvature_kasa_3pt(x, y, offset=10):
    """
    Curvature from circle-through-3-points (equivalent to 3-point Kåsa usage here).
    Uses points i-offset, i, i+offset. Endpoints become NaN.
    """
    x = np.asarray(x, dtype=float)
    y = np.asarray(y, dtype=float)
    N = len(x)

    if 2 * offset >= N:
        raise ValueError(f"offset={offset} too large for N={N} points.")

    kappa = np.full(N, np.nan)

    for i in range(offset, N - offset):
        x1, y1 = x[i - offset], y[i - offset]
        x2, y2 = x[i], y[i]
        x3, y3 = x[i + offset], y[i + offset]

        a = np.hypot(x2 - x1, y2 - y1)
        b = np.hypot(x3 - x2, y3 - y2)
        c = np.hypot(x1 - x3, y1 - y3)

        area2 = abs((x2 - x1) * (y3 - y1) - (y2 - y1) * (x3 - x1))

        denom = a * b * c
        if area2 > 1e-12 and denom > 1e-12:
            kappa[i] = 2.0 * area2 / denom
        else:
            kappa[i] = 0.0

    return kappa


# -------- MAIN --------
paths = sorted(glob.glob("mutant_primordia_shape/*.csv"))[:6]  # pick six
N_POINTS = 100
OFFSET = 10

curvatures = []
for p in paths:
    x_p, y_p = load_process_parameterize(p, n_points=N_POINTS)
    kappa = curvature_kasa_3pt(x_p, y_p, offset=OFFSET)
    curvatures.append(kappa)

curvatures = np.stack(curvatures, axis=0)  # (M, N)

kappa_mean = np.nanmean(curvatures, axis=0)
kappa_sd = np.nanstd(curvatures, axis=0)

idx = np.arange(N_POINTS)

MEAN_GREEN = (0.0, 0.39, 0.0)
SD_GREEN = (0.56, 0.93, 0.56)

plt.figure()
plt.plot(idx, kappa_mean, linewidth=2, label="Mean curvature", color = MEAN_GREEN)
plt.fill_between(idx, kappa_mean - kappa_sd, kappa_mean + kappa_sd, alpha=0.4, label="± SD", color=SD_GREEN)

plt.ylim(0, 6)
plt.yticks([0, 2, 4, 6])

plt.xlabel("Boundary point index (0..99)")
plt.ylabel("Curvature (1 / normalized length)")
plt.title(f"Curvature (Kåsa 3-point, offset={OFFSET}) mean ± SD (n={curvatures.shape[0]})")
plt.legend(frameon=False)

plt.savefig("mutant_curvature_mean_sd.png", dpi=1000, bbox_inches="tight")
plt.show()
