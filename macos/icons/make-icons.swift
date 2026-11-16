// Regenerate the input method icons:
//
//   xcrun swift macos/icons/make-icons.swift macos/ChewingPlus
//
// Writes chewing-plus.pdf, used for the input method, the input menu and
// the Keyboard settings list. The glyph is turned into a path, so the PDF
// does not embed a font.
import CoreGraphics
import CoreText
import Foundation

let outDir = URL(fileURLWithPath: CommandLine.arguments.count > 1
    ? CommandLine.arguments[1] : "macos/ChewingPlus")

func glyphPath(_ text: String, fontNames: [String], size: CGFloat) -> CGPath {
    var font: CTFont?
    for name in fontNames {
        let candidate = CTFontCreateWithName(name as CFString, size, nil)
        if (CTFontCopyPostScriptName(candidate) as String) == name {
            font = candidate
            break
        }
    }
    let ctFont = font ?? CTFontCreateUIFontForLanguage(.system, size, "zh-Hant" as CFString)!
    let attr = NSAttributedString(string: text, attributes: [
        NSAttributedString.Key(kCTFontAttributeName as String): ctFont,
    ])
    let line = CTLineCreateWithAttributedString(attr)
    let path = CGMutablePath()
    for run in CTLineGetGlyphRuns(line) as! [CTRun] {
        let runFont = (CTRunGetAttributes(run) as NSDictionary)[kCTFontAttributeName] as! CTFont
        let count = CTRunGetGlyphCount(run)
        var glyphs = [CGGlyph](repeating: 0, count: count)
        var positions = [CGPoint](repeating: .zero, count: count)
        CTRunGetGlyphs(run, CFRange(), &glyphs)
        CTRunGetPositions(run, CFRange(), &positions)
        for i in 0..<count {
            if let g = CTFontCreatePathForGlyph(runFont, glyphs[i], nil) {
                path.addPath(g, transform: CGAffineTransform(
                    translationX: positions[i].x, y: positions[i].y))
            }
        }
    }
    return path
}

/// Moves `path` so its bounding box is centred on `center`.
func centred(_ path: CGPath, at center: CGPoint) -> CGPath {
    let box = path.boundingBoxOfPath
    var t = CGAffineTransform(translationX: center.x - box.midX, y: center.y - box.midY)
    return path.copy(using: &t)!
}

let fonts = ["PingFangTC-Semibold", "PingFangTC-Medium", "HiraginoSansGB-W6", "STHeitiTC-Medium"]

func writePDF(_ name: String, size: CGFloat, draw: (CGContext) -> Void) {
    var box = CGRect(x: 0, y: 0, width: size, height: size)
    let url = outDir.appendingPathComponent(name)
    let ctx = CGContext(url as CFURL, mediaBox: &box, nil)!
    ctx.beginPDFPage(nil)
    draw(ctx)
    ctx.endPDFPage()
    ctx.closePDF()
    print("wrote \(url.path)")
}

// Colour icon: chewing orange, white 酷, a small + badge.
writePDF("chewing-plus.pdf", size: 64) { ctx in
    let tile = CGPath(roundedRect: CGRect(x: 2, y: 2, width: 60, height: 60),
                      cornerWidth: 14, cornerHeight: 14, transform: nil)
    ctx.addPath(tile)
    ctx.clip()
    let colors = [CGColor(red: 1.0, green: 0.62, blue: 0.20, alpha: 1),
                  CGColor(red: 0.93, green: 0.36, blue: 0.10, alpha: 1)] as CFArray
    let gradient = CGGradient(colorsSpace: CGColorSpaceCreateDeviceRGB(),
                              colors: colors, locations: [0, 1])!
    ctx.drawLinearGradient(gradient, start: CGPoint(x: 0, y: 62),
                           end: CGPoint(x: 0, y: 2), options: [])
    ctx.resetClip()
    ctx.setFillColor(CGColor(red: 1, green: 1, blue: 1, alpha: 1))
    ctx.addPath(centred(glyphPath("酷", fontNames: fonts, size: 34),
                        at: CGPoint(x: 29, y: 35)))
    ctx.fillPath()
    // + badge, bottom right
    ctx.setFillColor(CGColor(red: 1, green: 1, blue: 1, alpha: 1))
    ctx.fillEllipse(in: CGRect(x: 43, y: 5, width: 16, height: 16))
    ctx.setFillColor(CGColor(red: 0.93, green: 0.36, blue: 0.10, alpha: 1))
    ctx.fill(CGRect(x: 49.5, y: 8, width: 3, height: 10))
    ctx.fill(CGRect(x: 46, y: 11.5, width: 10, height: 3))
}
