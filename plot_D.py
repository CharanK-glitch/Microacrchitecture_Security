import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

print("Loading deviation matrix...")

try:
    df = pd.read_csv("D_profile_matrix.csv", index_col=0)
    D = df.to_numpy()
except FileNotFoundError:
    print("Error: D_deviation_profile.csv not found. Run profiler first.")
    exit(1)

print(f"Matrix shape: {D.shape}")
print(f"Min deviation: {D.min():.3f} cycles")
print(f"Max deviation: {D.max():.3f} cycles")
print(f"Mean |deviation|: {np.mean(np.abs(D)):.3f} cycles\n")


plt.figure(figsize=(14, 5.5))
sns.heatmap(
    D,
    cmap='coolwarm',
    center=0,
    vmin=-3, vmax=3,                    
    xticklabels=16,
    yticklabels=True,
    cbar_kws={'label': 'Deviation (cycles)'},
    linewidths=0.15,
    linecolor='gray'
)
plt.xlabel('Plaintext byte value (0–255)')
plt.ylabel('Byte position (0–15)')
plt.title('Deviation Profile D[i][j] – T-table AES-128\n'
          '(small values due to modern CPU prefetching)')
plt.tight_layout()
plt.savefig("1_deviation_heatmap.png", dpi=300, bbox_inches='tight')
print("Saved: 1_deviation_heatmap.png")


plt.figure(figsize=(10, 4))
abs_D = np.abs(D)
mean_abs = np.mean(abs_D, axis=1)
plt.bar(range(16), mean_abs, color='teal', alpha=0.85, edgecolor='black')
plt.xlabel('Byte position (0–15)')
plt.ylabel('Average |D[i][j]| (cycles)')
plt.title('Average Absolute Deviation per Plaintext Position')
plt.xticks(range(16))
plt.grid(axis='y', linestyle='--', alpha=0.5)
plt.tight_layout()
plt.savefig("2_avg_abs_per_position.png", dpi=300, bbox_inches='tight')
print("Saved: 2_avg_abs_per_position.png")


plt.figure(figsize=(14, 6))
positions = [0, 4, 8, 12]  # example positions – change if you want others
for pos in positions:
    plt.plot(D[pos, :], label=f'Position {pos}', linewidth=1.2, alpha=0.9)

plt.xlabel('Byte value (0–255)')
plt.ylabel('Deviation D[i][j] (cycles)')
plt.title('Deviation Profiles for Selected Byte Positions\n'
          '(look for any faint periodicity ~64)')
plt.legend()
plt.grid(True, linestyle='--', alpha=0.4)
plt.tight_layout()
plt.savefig("3_selected_rows_lineplot.png", dpi=300, bbox_inches='tight')
print("Saved: 3_selected_rows_lineplot.png")


plt.figure(figsize=(9, 5))
plt.hist(D.flatten(), bins=120, color='cornflowerblue', edgecolor='black', alpha=0.75)
plt.xlabel('Deviation D[i][j] (cycles)')
plt.ylabel('Count')
plt.title('Distribution of All 4096 Deviation Values')
plt.grid(axis='y', linestyle='--', alpha=0.5)
plt.tight_layout()
plt.savefig("4_deviation_histogram.png", dpi=300, bbox_inches='tight')
print("Saved: 4_deviation_histogram.png")


plt.figure(figsize=(12, 5))
sns.boxplot(data=D.T, width=0.6, fliersize=2, linewidth=1.2)
plt.xlabel('Byte position (0–15)')
plt.ylabel('Deviation D[i][j] (cycles)')
plt.title('Boxplot of Deviations by Byte Position\n'
          '(shows range and outliers)')
plt.tight_layout()
plt.savefig("5_boxplot_per_position.png", dpi=300, bbox_inches='tight')
print("Saved: 5_boxplot_per_position.png")


plt.show()  