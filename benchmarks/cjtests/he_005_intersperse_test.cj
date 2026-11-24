import std.collection.ArrayList
import std.unittest.*
import std.unittest.testmacro.*

func listOf(items: Array<Int64>): ArrayList<Int64> {
    let out = ArrayList<Int64>()
    for (item in items) {
        out.add(item)
    }
    return out
}

@Test
func case0() {
    @Expect(typed_fn_solve(listOf([]), 7), listOf([]))
}

@Test
func case1() {
    @Expect(typed_fn_solve(listOf([5, 6, 3, 2]), 8), listOf([5, 8, 6, 8, 3, 8, 2]))
}

@Test
func case2() {
    @Expect(typed_fn_solve(listOf([2, 2, 2]), 2), listOf([2, 2, 2, 2, 2]))
}

@Test
func case3() {
    @Expect(typed_fn_solve(listOf([4]), 9), listOf([4]))
}

@Test
func case4() {
    @Expect(typed_fn_solve(listOf([1, 2]), 0), listOf([1, 0, 2]))
}

@Test
func case5() {
    @Expect(typed_fn_solve(listOf([]), -1), listOf([]))
}

@Test
func case6() {
    @Expect(typed_fn_solve(listOf([7, 7]), -3), listOf([7, -3, 7]))
}
