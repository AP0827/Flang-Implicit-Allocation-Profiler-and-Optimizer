import unittest

from flang_alloc_profiler.analyzer import AllocationAnalyzer
from flang_alloc_profiler.models import AllocationClassification, AllocationKind, AnalysisRecord, SourceLocation
from flang_alloc_profiler.transform import rewrite_simple_temporary


class AllocationAnalyzerTests(unittest.TestCase):
    def test_classifies_realloc_shape_mismatch_as_necessary(self) -> None:
        analyzer = AllocationAnalyzer()
        record = AnalysisRecord(
            kind=AllocationKind.REALLOC_ON_ASSIGNMENT,
            location=SourceLocation("sample.f90", 12),
            expression="A = B",
            source_construct="assignment",
            shape_match=False,
        )

        summary = analyzer.analyze([record])
        self.assertIs(summary.sites[0].classification, AllocationClassification.NECESSARY)

    def test_classifies_temp_with_stack_safety_as_provably_unnecessary(self) -> None:
        analyzer = AllocationAnalyzer()
        record = AnalysisRecord(
            kind=AllocationKind.TEMPORARY_ARRAY,
            location=SourceLocation("sample.f90", 8),
            expression="B + C",
            source_construct="array expression",
            provable_stack_safety=True,
            bytes_allocated=1024,
        )

        summary = analyzer.analyze([record])
        self.assertIs(summary.sites[0].classification, AllocationClassification.PROVABLY_UNNECESSARY)


class TransformationTests(unittest.TestCase):
    def test_rewrites_simple_array_assignment(self) -> None:
        analyzer = AllocationAnalyzer()
        record = AnalysisRecord(
            kind=AllocationKind.TEMPORARY_ARRAY,
            location=SourceLocation("sample.f90", 1),
            expression="B + C",
            source_construct="array expression",
            provable_stack_safety=True,
        )
        site = analyzer.analyze([record]).sites[0]
        result = rewrite_simple_temporary("A = B + C\n", site)

        self.assertIn("do concurrent", result.source)
        self.assertIn("A(i) = B(i) + C(i)", result.source)
        self.assertTrue(result.suggestions)
