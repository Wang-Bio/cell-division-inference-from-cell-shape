import glob
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.patches import Patch
from matplotlib.lines import Line2D

def resample_contour_equal_arc_length(x, y, n_points=100, closed=False):
    x = np.asarray(x, dtype=float).ravel()
    y = np.asarray(y, dtype=float).ravel()
    if x.size != y.size or x.size < 2:
        raise ValueError("x and y must have the same length and contain at least 2 points.")

    # (do NOT close for open contours)
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
    df = pd.read_csv(csv_path, header=None, names=["id", "x", "y"])
    df["x"] = pd.to_numeric(df["x"], errors="coerce")
    df["y"] = pd.to_numeric(df["y"], errors="coerce")
    # your normalization
    y_min = df["y"].min()
    y_max = df["y"].max()
    y_range = y_max - y_min
    if y_range == 0:
        raise ValueError(f"{csv_path}: y_range is zero.")

    x_mean = df["x"].mean()

    x_norm = (df["x"] - x_mean) / y_range
    y_norm = (df["y"] - y_min) / y_range

    # flip y
    y_flip = 1.0 - y_norm

    # resample to 100 equally spaced points along arc length (OPEN)
    x_p, y_p = resample_contour_equal_arc_length(x_norm, y_flip, n_points=n_points, closed=False)
    return x_p, y_p


def mean_and_sd_band_open(contours_xy):
    """
    contours_xy: (M, N, 2) open contours, matched index-wise.
    Returns mean (N,2) and upper/lower band (N,2) using SD along local normal.
    """
    M, N, _ = contours_xy.shape
    mean = contours_xy.mean(axis=0)  # (N,2)

    # covariance per point index
    covs = np.zeros((N, 2, 2))
    for i in range(N):
        covs[i] = np.cov(contours_xy[:, i, :].T, bias=False)

    upper = np.zeros_like(mean)
    lower = np.zeros_like(mean)

    for i in range(N):
        # tangent for open curve: forward/backward at ends, central elsewhere
        if i == 0:
            tangent = mean[1] - mean[0]
        elif i == N - 1:
            tangent = mean[N - 1] - mean[N - 2]
        else:
            tangent = mean[i + 1] - mean[i - 1]

        tnorm = np.linalg.norm(tangent)
        if tnorm < 1e-12:
            n = np.array([0.0, 1.0])
        else:
            t = tangent / tnorm
            n = np.array([-t[1], t[0]])  # normal

        var_n = float(n.T @ covs[i] @ n)
        sd_n = np.sqrt(max(var_n, 0.0))

        upper[i] = mean[i] + sd_n * n
        lower[i] = mean[i] - sd_n * n

    return mean, upper, lower


# -------- MAIN --------
# Put your six paths here, or use a glob pattern:
paths = sorted(glob.glob("WT_primordia_shape/*.csv"))[:6]  # first six found
# OR explicitly:
# paths = [".../a.csv", ".../b.csv", ".../c.csv", ".../d.csv", ".../e.csv", ".../f.csv"]

N_POINTS = 100

contours = []
for p in paths:
    x_p, y_p = load_process_parameterize(p, n_points=N_POINTS)
    contours.append(np.stack([x_p, y_p], axis=1))  # (N,2)

contours = np.stack(contours, axis=0)  # (M,N,2)

mean, upper, lower = mean_and_sd_band_open(contours)

plt.figure()

# all contours (faint)
#for k in range(contours.shape[0]):
#    plt.plot(contours[k, :, 0], contours[k, :, 1], alpha=0.25)

MEAN_GREEN = (0.0, 0.39, 0.0)
SD_GREEN = (0.56, 0.93, 0.56)

# SD band
band_x = np.concatenate([upper[:, 0], lower[::-1, 0]])
band_y = np.concatenate([upper[:, 1], lower[::-1, 1]])
plt.fill(band_x, band_y, alpha=0.4, color=SD_GREEN)

# mean
plt.plot(mean[:, 0], mean[:, 1], linewidth=2, color=MEAN_GREEN)

legend_handles = [
    Line2D([0], [0], color=MEAN_GREEN, linewidth=2, label="Mean contour"),
    Patch(facecolor=SD_GREEN, alpha=0.4, label="± SD")
]

plt.legend(handles=legend_handles, frameon=False)

plt.xlabel("x (normalized)")
plt.ylabel("y (normalized, flipped)")
plt.title(f"Mean open contour (n={contours.shape[0]}) with SD band")
plt.gca().set_aspect("equal", adjustable="box")
plt.savefig("WT_mean_contour_with_sd.png",dpi=1000,bbox_inches="tight")
plt.show()
