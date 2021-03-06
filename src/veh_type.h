#ifndef VEH_TYPE_H
#define VEH_TYPE_H

#include "color.h"
#include "string_id.h"
#include "int_id.h"

#include <vector>

struct vpart_info;
using vpart_str_id = string_id<vpart_info>;
using vpart_id = int_id<vpart_info>;

class JsonObject;

/**
 * Represents an entry in the breaks_into list.
 */
struct break_entry {
    std::string item_id;
    int min;
    int max;
};

// bitmask backing store of -certian- vpart_info.flags, ones that
// won't be going away, are involved in core functionality, and are checked frequently
enum vpart_bitflags : int {
    VPFLAG_ARMOR,
    VPFLAG_EVENTURN,
    VPFLAG_ODDTURN,
    VPFLAG_CONE_LIGHT,
    VPFLAG_CIRCLE_LIGHT,
    VPFLAG_BOARDABLE,
    VPFLAG_AISLE,
    VPFLAG_CONTROLS,
    VPFLAG_OBSTACLE,
    VPFLAG_OPAQUE,
    VPFLAG_OPENABLE,
    VPFLAG_SEATBELT,
    VPFLAG_WHEEL,
    VPFLAG_MOUNTABLE,
    VPFLAG_FLOATS,
    VPFLAG_DOME_LIGHT,
    VPFLAG_AISLE_LIGHT,
    VPFLAG_ATOMIC_LIGHT,
    VPFLAG_ALTERNATOR,
    VPFLAG_ENGINE,
    VPFLAG_FRIDGE,
    VPFLAG_FUEL_TANK,
    VPFLAG_LIGHT,
    VPFLAG_WINDOW,
    VPFLAG_CURTAIN,
    VPFLAG_CARGO,
    VPFLAG_INTERNAL,
    VPFLAG_SOLAR_PANEL,
    VPFLAG_VARIABLE_SIZE,
    VPFLAG_TRACK,
    VPFLAG_RECHARGE,
    VPFLAG_EXTENDS_VISION,

    NUM_VPFLAGS
};
/* Flag info:
 * INTERNAL - Can be mounted inside other parts
 * ANCHOR_POINT - Allows secure seatbelt attachment
 * OVER - Can be mounted over other parts
 * VARIABLE_SIZE - Has 'bigness' for power, wheel radius, etc
 * MOUNTABLE - Usable as a point to fire a mountable weapon from.
 * Other flags are self-explanatory in their names. */
struct vpart_info {
    using itype_id = std::string;
    vpart_str_id id;         // unique identifier for this part
    vpart_id loadid;             // # of loaded order, non-saved runtime optimization
    std::string name;       // part name, user-visible
    long sym;               // symbol of part as if it's looking north
    nc_color color;         // color
    char sym_broken;        // symbol of broken part as if it's looking north
    nc_color color_broken;  // color of broken part
    int dmg_mod;            // damage modifier, percent
    int durability;         // durability
    int power;              // engine (top spd), solar panel/powered component (% of 1 fuel per turn, can be > 100)
    int epower;             // electrical power in watts (positive values for generation, negative for consumption)
    int folded_volume;      // volume of a foldable part when folded
    int range;              // turret target finder range
    union {
        int par1;
        int size;       // fuel tank, trunk
        int wheel_width;// wheel width in inches. car could be 9, bicycle could be 2.
        int bonus;      // seatbelt (str), muffler (%), horn (vol)
    };
    std::string fuel_type;  // engine, fuel tank
    itype_id item;      // corresponding item
    int difficulty;     // installation difficulty (mechanics requirement)
    std::string location;   //Where in the vehicle this part goes
    std::vector<break_entry> breaks_into;
private:
    std::set<std::string> flags;    // flags
    std::bitset<NUM_VPFLAGS> bitflags; // flags checked so often that things slow down due to string cmp
public:

    int z_order;        // z-ordering, inferred from location, cached here
    int list_order;     // Display order in vehicle interact display

    bool has_flag(const std::string &flag) const
    {
        return flags.count(flag) != 0;
    }
    bool has_flag(const vpart_bitflags flag) const
    {
        return bitflags.test( flag );
    }
    void set_flag( const std::string &flag );

    static void load( JsonObject &jo );
    static void check();
    static void reset();

    static const std::vector<const vpart_info*> &get_all();
    /**
     * The id of the null-part. The part should not actually be used, but its id can be used like
     * a null-pointer. Note that the null-part is still a completely valid part, getting the
     * vpart_info object of this id will not issue a debug message.
     */
    static const vpart_str_id null;
};

extern const vpart_str_id legacy_vpart_id[74];

#endif
