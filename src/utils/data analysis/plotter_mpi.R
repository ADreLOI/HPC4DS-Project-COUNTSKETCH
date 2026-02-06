# =============================================================================
# MPI Count Sketch - Professional R Plotter (Enhanced)
# =============================================================================
# 
# Usage: Run in R terminal from project root:
#   source("src/utils/data analysis/plotter_mpi.R")
#
# Required packages (install first time):
#   install.packages(c("ggplot2", "dplyr", "tidyr", "scales"))
#
# Output: charts/*.png
# =============================================================================

# --- Setup -------------------------------------------------------------------
library(ggplot2)
library(dplyr)
library(scales)

# Create charts directory
dir.create("charts", showWarnings = FALSE)

# Publication theme with enhanced styling
theme_publication <- function() {
  theme_minimal(base_size = 12) +
  theme(
    plot.title = element_text(face = "bold", size = 14, hjust = 0.5),
    plot.subtitle = element_text(face = "italic", size = 10, hjust = 0.5, color = "gray40"),
    axis.title = element_text(face = "bold", size = 11),
    axis.text = element_text(size = 10),
    legend.position = "top",
    legend.title = element_blank(),
    legend.text = element_text(size = 10),
    panel.grid.minor = element_blank(),
    panel.grid.major = element_line(color = "gray90"),
    panel.border = element_rect(color = "grey70", fill = NA, linewidth = 0.5),
    plot.margin = margin(10, 15, 10, 10)
  )
}

# Color palette
col_compute <- "#2E86AB"
col_comm <- "#3CB371"
col_ideal <- "#E63946"
colors_size <- c("500K" = "#2E86AB", "1M" = "#F18F01", "8M" = "#A23B72")

# =============================================================================
# 1. STRONG SCALING PLOTS
# =============================================================================

# Load 8M strong scaling data
perf_8m <- read.csv("results/performance_mpi_8388608.csv")

# --- Plot 1: Strong Scaling Speedup (Compute Only) ---------------------------
p1 <- ggplot(perf_8m, aes(x = Processes)) +
  geom_line(aes(y = Speedup_Compute, color = "MPI Speedup", linetype = "MPI Speedup"), linewidth = 1.2) +
  geom_point(aes(y = Speedup_Compute, color = "MPI Speedup"), size = 3) +
  geom_line(aes(y = Processes, color = "Ideal Linear", linetype = "Ideal Linear"), linewidth = 1) +
  scale_x_continuous(breaks = perf_8m$Processes, trans = "log2") +
  scale_y_continuous(breaks = seq(0, 70, 10)) +
  scale_color_manual(values = c("MPI Speedup" = col_compute, "Ideal Linear" = col_ideal)) +
  scale_linetype_manual(values = c("MPI Speedup" = "solid", "Ideal Linear" = "dashed")) +
  labs(
    title = "Strong Scaling: Speedup (Compute Only)",
    subtitle = expression(paste("8M items | Formula: ", S[p], " = ", T[1], " / ", T[p])),
    x = "Number of MPI Processes",
    y = "Speedup Factor"
  ) +
  theme_publication()

ggsave("charts/strong_scaling_speedup_compute.png", p1, width = 10, height = 6, dpi = 300)
print("Saved: strong_scaling_speedup_compute.png")

# --- Plot 2: Strong Scaling Speedup (With Communication) ---------------------
p2 <- ggplot(perf_8m, aes(x = Processes)) +
  geom_line(aes(y = Speedup_Comm, color = "MPI Speedup", linetype = "MPI Speedup"), linewidth = 1.2) +
  geom_point(aes(y = Speedup_Comm, color = "MPI Speedup"), size = 3) +
  geom_line(aes(y = Processes, color = "Ideal Linear", linetype = "Ideal Linear"), linewidth = 1) +
  scale_x_continuous(breaks = perf_8m$Processes, trans = "log2") +
  scale_y_continuous(breaks = seq(0, 70, 10)) +
  scale_color_manual(values = c("MPI Speedup" = col_comm, "Ideal Linear" = col_ideal)) +
  scale_linetype_manual(values = c("MPI Speedup" = "solid", "Ideal Linear" = "dashed")) +
  labs(
    title = "Strong Scaling: Speedup (With Communication)",
    subtitle = "8M items | Includes MPI communication overhead",
    x = "Number of MPI Processes",
    y = "Speedup Factor"
  ) +
  theme_publication()

ggsave("charts/strong_scaling_speedup.png", p2, width = 10, height = 6, dpi = 300)
print("Saved: strong_scaling_speedup.png")

# --- Plot 3: Strong Scaling Efficiency ----------------------------------------
p3 <- ggplot(perf_8m, aes(x = Processes)) +
  geom_line(aes(y = Efficiency_Compute, color = "Compute Only"), linewidth = 1.2) +
  geom_point(aes(y = Efficiency_Compute, color = "Compute Only"), size = 3) +
  geom_line(aes(y = Efficiency_Comm, color = "With Communication"), linewidth = 1.2) +
  geom_point(aes(y = Efficiency_Comm, color = "With Communication"), size = 3) +
  geom_hline(yintercept = 100, linetype = "dashed", color = col_ideal, linewidth = 1) +
  scale_x_continuous(breaks = perf_8m$Processes, trans = "log2") +
  scale_y_continuous(limits = c(0, 120)) +
  scale_color_manual(values = c("Compute Only" = col_compute, "With Communication" = col_comm)) +
  labs(
    title = "Strong Scaling: Efficiency",
    subtitle = expression(paste("8M items | Formula: ", E[p], " = ", S[p], " / p × 100% | Goal: Stay close to 100%")),
    x = "Number of MPI Processes",
    y = "Efficiency (%)"
  ) +
  annotate("text", x = 48, y = 104, label = "Ideal (100%)", color = col_ideal, fontface = "italic", size = 3.5) +
  theme_publication()

ggsave("charts/strong_scaling_efficiency.png", p3, width = 10, height = 6, dpi = 300)
print("Saved: strong_scaling_efficiency.png")

# --- Plot 4: Strong Scaling Time ---------------------------------------------
p4 <- ggplot(perf_8m, aes(x = Processes)) +
  geom_line(aes(y = Compute_Time, color = "Compute Only"), linewidth = 1.2) +
  geom_point(aes(y = Compute_Time, color = "Compute Only"), size = 3) +
  geom_line(aes(y = Total_Time, color = "Total (with comm)"), linewidth = 1.2) +
  geom_point(aes(y = Total_Time, color = "Total (with comm)"), size = 3) +
  scale_x_continuous(breaks = perf_8m$Processes, trans = "log2") +
  scale_y_log10(labels = scales::number_format(accuracy = 0.01)) +
  scale_color_manual(values = c("Compute Only" = col_compute, "Total (with comm)" = col_comm)) +
  labs(
    title = "Strong Scaling: Execution Time",
    subtitle = "8M items | Communication overhead gap increases with process count",
    x = "Number of MPI Processes",
    y = "Time (seconds, log scale)"
  ) +
  theme_publication()

ggsave("charts/strong_scaling_time.png", p4, width = 10, height = 6, dpi = 300)
print("Saved: strong_scaling_time.png")

# =============================================================================
# 2. WEAK SCALING PLOTS
# =============================================================================

# Load 8M weak scaling data
weak_8m <- read.csv("results/weak_scaling_mpi_8388608.csv")

# Calculate CORRECT weak scaling efficiency: T1_base / Tp
weak_8m <- weak_8m %>%
  mutate(
    T1_base = Serial_Time / Processes,
    Efficiency_Compute = T1_base / Compute_Time,
    Efficiency_Comm = T1_base / Compute_Communication_Time
  )

# --- Plot 5: Weak Scaling Efficiency (Compute Only) --------------------------
p5 <- ggplot(weak_8m, aes(x = Processes)) +
  geom_line(aes(y = Efficiency_Compute, color = "Measured Efficiency"), linewidth = 1.2) +
  geom_point(aes(y = Efficiency_Compute, color = "Measured Efficiency"), size = 3) +
  geom_hline(aes(yintercept = 1, color = "Ideal Scaling (1.0)"), linetype = "dashed", linewidth = 1) +
  scale_x_continuous(breaks = weak_8m$Processes, trans = "log2") +
  scale_y_continuous(limits = c(0, 1.4)) +
  scale_color_manual(values = c("Measured Efficiency" = col_compute, "Ideal Scaling (1.0)" = col_ideal)) +
  labs(
    title = "Weak Scaling: Efficiency (Compute Only)",
    subtitle = expression(paste("Formula: ", S[w], " = ", T[1](N), " / ", T[p](N %.% p), " | Goal: Stay close to 1.0")),
    x = "Number of MPI Processes",
    y = expression(paste("Efficiency (", T[1], " / ", T[p], ")"))
  ) +
  theme_publication()

ggsave("charts/weak_scaling_efficiency_compute.png", p5, width = 10, height = 6, dpi = 300)
print("Saved: weak_scaling_efficiency_compute.png")

# --- Plot 6: Weak Scaling Efficiency (With Communication) --------------------
p6 <- ggplot(weak_8m, aes(x = Processes)) +
  geom_line(aes(y = Efficiency_Comm, color = "Measured Weak Efficiency"), linewidth = 1.2) +
  geom_point(aes(y = Efficiency_Comm, color = "Measured Weak Efficiency"), size = 3) +
  geom_hline(aes(yintercept = 1, color = "Ideal Scaling (1.0)"), linetype = "dashed", linewidth = 1) +
  scale_x_continuous(breaks = weak_8m$Processes, trans = "log2") +
  scale_y_continuous(limits = c(0, 1.4)) +
  scale_color_manual(values = c("Measured Weak Efficiency" = col_comm, "Ideal Scaling (1.0)" = col_ideal)) +
  labs(
    title = "Weak Scaling: Efficiency vs. Processes",
    subtitle = "Includes communication overhead | Goal: Stay close to 1.0",
    x = "Number of MPI Processes",
    y = expression(paste("Efficiency (", T[1], " / ", T[p], ")"))
  ) +
  theme_publication()

ggsave("charts/weak_scaling_efficiency.png", p6, width = 10, height = 6, dpi = 300)
print("Saved: weak_scaling_efficiency.png")

# --- Plot 7: Weak Scaling Times ---------------------------------------------
p7 <- ggplot(weak_8m, aes(x = Processes)) +
  geom_line(aes(y = Compute_Time, color = "Compute Only"), linewidth = 1.2) +
  geom_point(aes(y = Compute_Time, color = "Compute Only"), size = 3) +
  geom_line(aes(y = Compute_Communication_Time, color = "Total (with comm)"), linewidth = 1.2) +
  geom_point(aes(y = Compute_Communication_Time, color = "Total (with comm)"), size = 3) +
  scale_x_continuous(breaks = weak_8m$Processes, trans = "log2") +
  scale_color_manual(values = c("Compute Only" = col_compute, "Total (with comm)" = col_comm)) +
  labs(
    title = "Weak Scaling: Execution Times",
    subtitle = "Ideal: constant time | Reality: communication overhead grows with P",
    x = "Number of MPI Processes",
    y = "Time (seconds)"
  ) +
  theme_publication()

ggsave("charts/weak_scaling_times.png", p7, width = 10, height = 6, dpi = 300)
print("Saved: weak_scaling_times.png")

# =============================================================================
# 3. COMPARISON PLOTS (Multiple Dataset Sizes)
# =============================================================================

# Load all strong scaling data
perf_500k <- read.csv("results/performance_mpi_524288.csv") %>% mutate(Size = "500K")
perf_1m <- read.csv("results/performance_mpi_1048576.csv") %>% mutate(Size = "1M")
perf_8m_comp <- perf_8m %>% mutate(Size = "8M")

perf_all <- bind_rows(perf_500k, perf_1m, perf_8m_comp)
perf_all$Size <- factor(perf_all$Size, levels = c("500K", "1M", "8M"))

# --- Plot 8: Strong Scaling Comparison (3 sizes) -----------------------------
p8 <- ggplot(perf_all, aes(x = Processes, y = Speedup_Comm, color = Size)) +
  geom_line(linewidth = 1.2) +
  geom_point(size = 3) +
  geom_line(aes(y = Processes), linetype = "dashed", color = col_ideal, linewidth = 1, show.legend = FALSE) +
  scale_x_continuous(breaks = c(2, 4, 8, 16, 32, 64), trans = "log2") +
  scale_color_manual(values = colors_size) +
  labs(
    title = "Strong Scaling Comparison: Speedup vs. Dataset Size",
    subtitle = "Larger datasets achieve better scaling | 500K shows degradation at 64 processes",
    x = "Number of MPI Processes",
    y = "Speedup Factor (with communication)",
    color = "Dataset Size"
  ) +
  theme_publication() +
  theme(legend.title = element_text(face = "bold"))

ggsave("charts/strong_comparison_mpi.png", p8, width = 10, height = 6, dpi = 300)
print("Saved: strong_comparison_mpi.png")

# --- Plot 9: Strong Scaling Efficiency Comparison ----------------------------
p9_eff <- ggplot(perf_all, aes(x = Processes, y = Efficiency_Comm, color = Size)) +
  geom_line(linewidth = 1.2) +
  geom_point(size = 3) +
  geom_hline(yintercept = 100, linetype = "dashed", color = col_ideal, linewidth = 1) +
  scale_x_continuous(breaks = c(2, 4, 8, 16, 32, 64), trans = "log2") +
  scale_color_manual(values = colors_size) +
  labs(
    title = "Strong Scaling: Efficiency Comparison",
    subtitle = "Communication overhead impact varies with problem size",
    x = "Number of MPI Processes",
    y = "Efficiency (%)",
    color = "Dataset Size"
  ) +
  annotate("text", x = 48, y = 105, label = "Ideal (100%)", color = col_ideal, fontface = "italic", size = 3.5) +
  theme_publication() +
  theme(legend.title = element_text(face = "bold"))

ggsave("charts/strong_efficiency_comparison.png", p9_eff, width = 10, height = 6, dpi = 300)
print("Saved: strong_efficiency_comparison.png")

# --- Plot 10: Weak Scaling Comparison (3 sizes) ------------------------------
weak_500k <- read.csv("results/weak_scaling_mpi_524288.csv") %>% mutate(Size = "500K")
weak_1m <- read.csv("results/weak_scaling_mpi_1048576.csv") %>% mutate(Size = "1M")
weak_8m_comp <- weak_8m %>% mutate(Size = "8M")

weak_all <- bind_rows(weak_500k, weak_1m, weak_8m_comp)
weak_all$Size <- factor(weak_all$Size, levels = c("500K", "1M", "8M"))

# Calculate efficiency for all sizes
weak_all <- weak_all %>%
  mutate(
    T1_base = Serial_Time / Processes,
    Efficiency = T1_base / Compute_Communication_Time
  )

p10 <- ggplot(weak_all, aes(x = Processes, y = Efficiency, color = Size)) +
  geom_line(linewidth = 1.2) +
  geom_point(size = 3) +
  geom_hline(yintercept = 1, linetype = "dashed", color = col_ideal, linewidth = 1) +
  scale_x_continuous(breaks = c(2, 4, 8, 16, 32, 64), trans = "log2") +
  scale_y_continuous(limits = c(0, 1.4)) +
  scale_color_manual(values = colors_size) +
  labs(
    title = "Weak Scaling Comparison: Efficiency vs. Dataset Size",
    subtitle = expression(paste("Formula: ", S[w], " = ", T[1](N), " / ", T[p](N %.% p), " | Goal: Stay close to 1.0")),
    x = "Number of MPI Processes",
    y = expression(paste("Efficiency (", T[1], " / ", T[p], ")")),
    color = "Base Dataset"
  ) +
  annotate("text", x = 48, y = 1.05, label = "Ideal (1.0)", color = col_ideal, fontface = "italic", size = 3.5) +
  theme_publication() +
  theme(legend.title = element_text(face = "bold"))

ggsave("charts/weak_comparison_mpi.png", p10, width = 10, height = 6, dpi = 300)
print("Saved: weak_comparison_mpi.png")

# =============================================================================
# DONE
# =============================================================================
cat("\n========================================\n")
cat("All 10 charts generated successfully!\n")
cat("Output folder: charts/\n")
cat("========================================\n")
cat("\nCharts saved:\n")
cat("  1. strong_scaling_speedup_compute.png\n")
cat("  2. strong_scaling_speedup.png\n")
cat("  3. strong_scaling_efficiency.png\n")
cat("  4. strong_scaling_time.png\n")
cat("  5. weak_scaling_efficiency_compute.png\n")
cat("  6. weak_scaling_efficiency.png\n")
cat("  7. weak_scaling_times.png\n")
cat("  8. strong_comparison_mpi.png\n")
cat("  9. strong_efficiency_comparison.png\n")
cat(" 10. weak_comparison_mpi.png\n")
