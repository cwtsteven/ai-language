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
    @Expect(typed_fn_solve(listOf([])), listOf([]))
}

@Test
func case1() {
    @Expect(typed_fn_solve(listOf([3, 2, 1])), listOf([4, 3, 2]))
}

@Test
func case2() {
    @Expect(typed_fn_solve(listOf([5, 2, 5, 2, 3, 3, 9, 0, 123])), listOf([6, 3, 6, 3, 4, 4, 10, 1, 124]))
}

@Test
func case3() {
    @Expect(typed_fn_solve(listOf([0])), listOf([1]))
}

@Test
func case4() {
    @Expect(typed_fn_solve(listOf([-1, 4])), listOf([0, 5]))
}

@Test
func case5() {
    @Expect(typed_fn_solve(listOf([150])), listOf([151]))
}
