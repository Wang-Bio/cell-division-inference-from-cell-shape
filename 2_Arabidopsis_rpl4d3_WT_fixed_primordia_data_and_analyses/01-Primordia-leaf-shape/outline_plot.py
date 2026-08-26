import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

df = pd.read_csv(
    "WT_primordia_shape/sample4.csv",
    header=None,
    names=["id", "x", "y"]
)

def resample_contour_equal_arc_length(x, y, n_points=100, closed=True):
    """
    Resample a 2D contour so that points are equally spaced by arc length
    using piecewise-linear interpolation.

    Parameters
    ----------
    x, y : array-like
        Original contour coordinates in order (around the boundary).
    n_points : int
        Number of output boundary points (e.g., 100).
    closed : bool
        If True, closes the contour by connecting last->first.

    Returns
    -------
    xr, yr : np.ndarray
        Resampled coordinates of length n_points.
    """
    x = np.asarray(x, dtype=float).ravel()
    y = np.asarray(y, dtype=float).ravel()
    if x.size != y.size or x.size < 2:
        raise ValueError("x and y must have the same length and contain at least 2 points.")

    # Close contour if requested (append first point to end if not already)
    if closed:
        if not (np.isclose(x[0], x[-1]) and np.isclose(y[0], y[-1])):
            x = np.append(x, x[0])
            y = np.append(y, y[0])

    # Segment lengths
    dx = np.diff(x)
    dy = np.diff(y)
    seglen = np.hypot(dx, dy)

    # Guard against duplicate points / zero-length segments
    keep = seglen > 1e-12
    if not np.all(keep):
        # Remove zero-length segments by filtering points
        # Keep point i if segment i (i->i+1) is non-zero; always keep the last point
        idx = np.concatenate([np.where(keep)[0], [len(seglen)]])
        x = x[idx]
        y = y[idx]
        dx = np.diff(x)
        dy = np.diff(y)
        seglen = np.hypot(dx, dy)

    if seglen.size == 0 or np.all(seglen <= 0):
        raise ValueError("Contour has zero total length (all points identical).")

    # Cumulative arc length parameter s
    s = np.concatenate([[0.0], np.cumsum(seglen)])
    total = s[-1]

    # Target equally spaced arc-length samples
    if closed:
        # For closed contours, avoid duplicating the first point at the end:
        s_target = np.linspace(0.0, total, n_points + 1)[:-1]
    else:
        s_target = np.linspace(0.0, total, n_points)

    # Interpolate x(s), y(s)
    xr = np.interp(s_target, s, x)
    yr = np.interp(s_target, s, y)

    return xr, yr


y_min = df["y"].min()
y_max = df["y"].max()
y_range = y_max - y_min

x_mean = df["x"].mean()

df["y_norm"] = (df["y"] - y_min) / y_range
df["x_norm"] = (df["x"] - x_mean) / y_range

df["y_norm_updown"] = (1.0-df["y_norm"])


x_parameterized, y_parameterized = resample_contour_equal_arc_length(df["x_norm"], df["y_norm_updown"], n_points=100, closed=False)


plt.figure()
plt.plot(x_parameterized,y_parameterized, marker='o')
plt.xlabel("x_parameterized")
plt.ylabel("y_parameterized")
plt.title("Parameterized X-Y Plot")
plt.gca().set_aspect('equal', adjustable='box')
plt.show()
