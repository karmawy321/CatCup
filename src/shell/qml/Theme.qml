pragma Singleton
import QtQuick

QtObject {
    id: root

    // ---- 2026 Obsidian Studio Color System ----
    readonly property color bgApp: "#090A0D"           // Ultra-deep root background
    readonly property color bgSidebar: "#0E1015"       // Side panels (Media Browser / Inspector)
    readonly property color bgSurface: "#13161F"       // Cards, headers, toolbar surfaces
    readonly property color bgElevated: "#1A1E2A"      // Elevated buttons, inputs, tabs
    readonly property color bgCard: "#161922"          // Inner cards, list item delegates
    readonly property color bgHover: "#232838"         // Hover state
    readonly property color bgActive: "#2C3347"        // Pressed/Active state
    readonly property color bgCanvas: "#050608"        // Cinema preview letterbox backing

    // Precision Borders & Lines
    readonly property color borderSubtle: "#1B202D"    // Low-contrast separators
    readonly property color borderMedium: "#262C3E"    // Inputs, cards, panels
    readonly property color borderHighlight: "#363F58" // Subtle light reflections
    readonly property color borderFocus: "#00E599"     // Active/Focused border

    // 2026 Neon Accents (CapCut / Cyberpunk Studio Aesthetic)
    readonly property color accent: "#00E599"          // Neon Emerald / Jade
    readonly property color accentHover: "#1AE8A3"
    readonly property color accentPressed: "#00C282"
    readonly property color accentGlow: "#2000E599"     // Subtle translucent glow

    readonly property color cyan: "#00C8FF"            // Electric Cyan (Playhead / Selection)
    readonly property color cyanGlow: "#4000C8FF"
    readonly property color purple: "#A855F7"          // Caption / Text track accent
    readonly property color orange: "#FB923C"          // Warning / Transition accent
    readonly property color red: "#FF4D4F"             // Delete / Error accent
    readonly property color blue: "#38BDF8"            // Info / Cut badge

    // Track Specific Palette
    readonly property color trackVideo: "#0E3E43"      // Video clips primary
    readonly property color trackVideoGrad: "#175B62"
    readonly property color trackAudio: "#1A472A"      // Audio clips primary
    readonly property color trackAudioGrad: "#26693E"
    readonly property color trackText: "#3E2368"       // Subtitle/Text clips primary
    readonly property color trackTextGrad: "#583294"

    // Typography
    readonly property color textPrimary: "#F9FAFB"     // Crisp White
    readonly property color textSecondary: "#9CA3AF"   // Muted Silver
    readonly property color textTertiary: "#6B7280"    // Dark Muted
    readonly property color textDisabled: "#4B5563"
    readonly property color textAccent: "#00E599"

    readonly property string fontMono: "Consolas, 'Cascadia Code', 'JetBrains Mono', monospace"
    readonly property string fontBody: "Segoe UI, -apple-system, BlinkMacSystemFont, Roboto, sans-serif"

    // Metrics & Radii
    readonly property int radiusSmall: 4
    readonly property int radiusMedium: 6
    readonly property int radiusLarge: 10
    readonly property int radiusPill: 999
}
