// SPDX-License-Identifier: MIT
using System.Reflection;

[assembly: AssemblyTitle("CompatibilityOS.GacFixture")]
[assembly: AssemblyDescription("CompatibilityOS Fusion GAC compatibility fixture")]
[assembly: AssemblyVersion("1.0.0.0")]
[assembly: AssemblyCulture("")]

namespace CompatibilityOS.GacFixture
{
    public sealed class Fixture
    {
        public static string Identity { get { return "CompatibilityOS.GacFixture"; } }
    }
}
