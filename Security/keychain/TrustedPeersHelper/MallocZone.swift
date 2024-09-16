import ICU_Private

func setupICUMallocZone() {
    let zone = malloc_create_zone(0, 0)
    malloc_set_zone_name(zone, "ICU")

    var status = U_ZERO_ERROR
    u_setMemoryFunctions(zone, icuAlloc, icuRealloc, icuFree, &status)
}

// typedef void *U_CALLCONV UMemAllocFn(const void *context, size_t size);
private func icuAlloc(zone: UnsafeRawPointer?, size: Int) -> UnsafeMutableRawPointer? {
    guard let zone = UnsafeMutableRawPointer(mutating: zone) else {
        fatalError("missing malloc zone")
    }
    return malloc_zone_malloc(zone.bindMemory(to: malloc_zone_t.self, capacity: 1), size)
}

// typedef void *U_CALLCONV UMemReallocFn(const void *context, void *mem, size_t size);
private func icuRealloc(zone: UnsafeRawPointer?, ptr: UnsafeMutableRawPointer?, size: Int) -> UnsafeMutableRawPointer? {
    guard let zone = UnsafeMutableRawPointer(mutating: zone) else {
        fatalError("missing malloc zone")
    }
    return malloc_zone_realloc(zone.bindMemory(to: malloc_zone_t.self, capacity: 1), ptr, size)
}

// typedef void  U_CALLCONV UMemFreeFn (const void *context, void *mem);
private func icuFree(zone: UnsafeRawPointer?, ptr: UnsafeMutableRawPointer?) {
    guard let zone = UnsafeMutableRawPointer(mutating: zone) else {
        fatalError("missing malloc zone")
    }
    malloc_zone_free(zone.bindMemory(to: malloc_zone_t.self, capacity: 1), ptr)
}
