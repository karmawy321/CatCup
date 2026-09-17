pragma Singleton
import QtQuick

QtObject {
    id: root

    // ---- Exact CapCut Desktop Color System (Sampled from native UI) ----
    // Base Canvas & Surfaces
    readonly property color bgApp: "#16161A"           // Root window & timeline background
    readonly property color bgSidebar: "#1E1E22"       // Media browser & details panel
    readonly property color bgSubSidebar: "#18181C"    // Leftmost navigation sub-rail
    readonly property color bgSurface: "#202024"       // Panel headers & floating bars
    readonly property color bgCard: "#121214"          // Media card & delegate background
    readonly property color bgElevated: "#28282E"      // Buttons, inputs, search box
    readonly property color bgHover: "#32323A"         // Button / delegate hover
    readonly property color bgActive: "#3C3C46"        // Active / pressed
    readonly property color bgCanvas: "#0D0D10"        // Player monitor backing

    // Borders & Hairlines
    readonly property color borderSubtle: "#26262C"    // Inactive dividers
    readonly property color borderMedium: "#303038"    // Card borders, button outlines
    readonly property color borderHighlight: "#42424E" // Hovered element borders
    readonly property color borderFocus: "#00C7D4"     // Focused / selected border

    // Signature CapCut Cyan-Teal Brand Accent
    readonly property color accent: "#00C7D4"          // Primary cyan-teal
    readonly property color accentHover: "#1ED6E2"
    readonly property color accentPressed: "#00B0BC"
    readonly property color accentGlow: "#3300C7D4"
    readonly property color accentBadge: "#163E48"

    // Functional State Colors
    readonly property color danger: "#EF4444"
    readonly property color dangerBg: "#4A1E20"
    readonly property color warning: "#F59E0B"
    readonly property color success: "#10B981"
    readonly property color purple: "#A855F7"

    // CapCut Timeline Clip Colors (Exact from Screenshot)
    readonly property color clipVideo: "#1C4049"       // Slate cyan-teal video block
    readonly property color clipVideoBorder: "#2A5A66"
    readonly property color clipAudio: "#1E3A2F"       // Spruce green audio block
    readonly property color clipAudioBorder: "#2C5243"
    readonly property color clipText: "#3A2A48"        // Subtle plum text block
    readonly property color clipTextBorder: "#523C66"
    readonly property color clipWarning: "#5C2424"     // Speed / warning block

    // Typography
    readonly property color textPrimary: "#FFFFFF"     // Crisp white
    readonly property color textSecondary: "#C4C4CD"   // High-contrast muted silver
    readonly property color textTertiary: "#9494A0"    // Readable secondary gray
    readonly property color textDisabled: "#555560"
    readonly property color textAccent: "#00C7D4"

    // Valid single font family names for Qt font engine (not CSS comma lists)
    readonly property string fontMono: "Consolas"
    readonly property string fontBody: "Segoe UI"
    readonly property string fontHeading: "Segoe UI"

    // Standardized typography scale
    readonly property int fontSizeMicro: 10
    readonly property int fontSizeCaption: 11
    readonly property int fontSizeBody: 12
    readonly property int fontSizeHeading: 14
    readonly property int fontSizeTitle: 16

    // Hit targets and spacing
    readonly property int controlHeightDefault: 32
    readonly property int controlHeightCompact: 28
    readonly property int iconSizeDefault: 16
    readonly property int iconSizeCompact: 14

    // Radii
    readonly property int radiusSmall: 3
    readonly property int radiusMedium: 5
    readonly property int radiusLarge: 8
    readonly property int radiusPill: 999

    // Timeline Specific Tokens
    readonly property color rulerBg: "#141418"
    readonly property color rulerTick: "#383844"
    readonly property color rulerText: "#767684"
    readonly property color playheadLaser: "#00C7D4"
    readonly property color playheadHandle: "#00C7D4"
    readonly property color playheadGlow: "#4D00C7D4"
    readonly property color trackHeaderBg: "#18181C"
    readonly property color trackLaneBg: "#121215"
    readonly property color trackDivider: "#1E1E24"
}
