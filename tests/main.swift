// Minimal test freymvorki — tashqi bogʻliqliksiz.
// Test fayllari `testlar` massiviga oʻz funksiyalarini qoʻshadi.

import Foundation

var xatolar = 0
var otganlar = 0
var testlar: [(String, () -> Void)] = []

func tekshir(_ nom: String, _ shart: @autoclosure () -> Bool) {
    if shart() {
        otganlar += 1
    } else {
        xatolar += 1
        print("  ✗ \(nom)")
    }
}

func tengmi<T: Equatable>(_ nom: String, _ olingan: T, _ kutilgan: T) {
    if olingan == kutilgan {
        otganlar += 1
    } else {
        xatolar += 1
        print("  ✗ \(nom)")
        print("    olingan:  \(olingan)")
        print("    kutilgan: \(kutilgan)")
    }
}

// Test fayllari bu funksiyani chaqirib oʻzini roʻyxatga qoʻshadi.
func testQosh(_ nom: String, _ f: @escaping () -> Void) {
    testlar.append((nom, f))
}

barchaTestlarniRoyxatgaQosh()

for (nom, f) in testlar {
    print("• \(nom)")
    f()
}

print("")
if xatolar == 0 {
    print("✓ \(otganlar) ta tekshiruv oʻtdi")
    exit(0)
} else {
    print("✗ \(xatolar) ta xato, \(otganlar) ta oʻtdi")
    exit(1)
}
