#include "activity_handlers.h"

#include "game.h"
#include "map.h"
#include "player.h"
#include "veh_interact.h"
#include "debug.h"
#include "translations.h"
#include "sounds.h"
#include "monstergenerator.h"
#include "iuse_actor.h"
#include "rng.h"
#include "mongroup.h"
#include "morale.h"
#include "messages.h"
#include "martialarts.h"
#include "itype.h"

#include <sstream>

#define dbg(x) DebugLog((DebugLevel)(x),D_GAME) << __FILE__ << ":" << __LINE__ << ": "

void activity_handlers::burrow_do_turn(player_activity *act, player *p)
{
    if( calendar::turn % MINUTES(1) == 0 ) { // each turn is too much
        //~ Sound of a Rat mutant burrowing!
        sounds::sound( act->placement, 10, _("ScratchCrunchScrabbleScurry.") );
        if( act->moves_left <= 91000 && act->moves_left > 89000 ) {
            p->add_msg_if_player(m_info, _("You figure it'll take about an hour and a half at this rate."));
        }
        if( act->moves_left <= 71000 && act->moves_left > 69000 ) {
            p->add_msg_if_player(m_info, _("About an hour left to go."));
        }
        if( act->moves_left <= 31000 && act->moves_left > 29000 ) {
            p->add_msg_if_player(m_info, _("Shouldn't be more than half an hour or so now!"));
        }
        if( act->moves_left <= 11000 && act->moves_left > 9000 ) {
            p->add_msg_if_player(m_info, _("Almost there! Ten more minutes of work and you'll be through."));
        }
    }
}


void activity_handlers::burrow_finish(player_activity *act, player *p)
{
    const tripoint &pos = act->placement;
    if( g->m.is_bashable(pos) && g->m.has_flag("SUPPORTS_ROOF", pos) &&
        g->m.ter(pos) != t_tree ) {
        // Tunneling through solid rock is hungry, sweaty, tiring, backbreaking work
        // Not quite as bad as the pickaxe, though
        p->hunger += 10;
        p->fatigue += 15;
        p->thirst += 10;
        p->mod_pain(3 * rng(1, 3));
        // Mining is construction work!
        p->practice("carpentry", 5);
    } else if( g->m.move_cost(pos) == 2 && g->get_levz() == 0 &&
               g->m.ter(pos) != t_dirt && g->m.ter(pos) != t_grass ) {
        //Breaking up concrete on the surface? not nearly as bad
        p->hunger += 5;
        p->fatigue += 10;
        p->thirst += 5;
    }
    g->m.destroy( pos, true );
}

bool butcher_cbm_item( const std::string &item, const tripoint &pos, const int age )
{
    //To see if it spawns a random additional CBM
    if( one_in( 2 ) ) { //The CBM works
        g->m.spawn_item( pos, item, 1, 0, age );
        return true;
    }
    //There is a burnt out CBM
    g->m.spawn_item( pos, "burnt_out_bionic", 1, 0, age );
    return false;
}

bool butcher_cbm_group( const std::string &group, const tripoint &pos, const int age )
{
    //To see if it spawns a random additional CBM
    if( one_in( 2 ) ) { //The CBM works
        g->m.put_items_from_loc( group, pos, age );
        return true;
    }
    //There is a burnt out CBM
    g->m.spawn_item( pos, "burnt_out_bionic", 1, 0, age);
    return false;
}

void activity_handlers::butcher_finish( player_activity *act, player *p )
{
    // corpses can disappear (rezzing!), so check for that
    if( static_cast<int>(g->m.i_at(p->pos()).size()) <= act->index ||
        !(g->m.i_at(p->pos())[act->index].is_corpse() ) ) {
        add_msg(m_info, _("There's no corpse to butcher!"));
        return;
    }
    mtype *corpse = g->m.i_at(p->pos())[act->index].get_mtype();
    std::vector<item> contents = g->m.i_at(p->pos())[act->index].contents;
    int age = g->m.i_at(p->pos())[act->index].bday;
    g->m.i_rem(p->pos(), act->index);
    int factor = p->butcher_factor();
    int pieces = 0, skins = 0, bones = 0, fats = 0, sinews = 0, feathers = 0;
    bool stomach = false;

    switch (corpse->size) {
    case MS_TINY:
        pieces = 1;
        skins = 1;
        bones = 1;
        fats = 1;
        sinews = 1;
        feathers = 2;
        break;
    case MS_SMALL:
        pieces = 2;
        skins = 2;
        bones = 4;
        fats = 2;
        sinews = 4;
        feathers = 6;
        break;
    case MS_MEDIUM:
        pieces = 4;
        skins = 4;
        bones = 9;
        fats = 4;
        sinews = 9;
        feathers = 11;
        break;
    case MS_LARGE:
        pieces = 8;
        skins = 8;
        bones = 14;
        fats = 8;
        sinews = 14;
        feathers = 17;
        break;
    case MS_HUGE:
        pieces = 16;
        skins = 16;
        bones = 21;
        fats = 16;
        sinews = 21;
        feathers = 24;
        break;
    }

    int sSkillLevel = p->skillLevel("survival");

    auto roll_butchery = [&] () {
        double skill_shift = 0.;
        skill_shift += rng_float( 0, sSkillLevel - 3 );
        skill_shift += rng_float( 0, p->dex_cur - 8 ) / 4.0;
        if( p->str_cur < 4 ) {
            skill_shift -= rng_float( 0, 5 * ( 4 - p->str_cur ) ) / 4.0;
        }

        if( factor < 0 ) {
            skill_shift -= rng_float( 0, -factor / 5.0 );
        }

        return static_cast<int>( skill_shift );
    };

    int practice = std::max( 0, 4 + pieces + roll_butchery());
    
    p->practice("survival", practice);

    // Lose some meat, skins, etc if the rolls are low
    pieces +=   std::min( 0, roll_butchery() );
    skins +=    std::min( 0, roll_butchery() - 4 );
    bones +=    std::min( 0, roll_butchery() - 2 );
    fats +=     std::min( 0, roll_butchery() - 4 );
    sinews +=   std::min( 0, roll_butchery() - 8 );
    feathers += std::min( 0, roll_butchery() - 1 );
    stomach = (roll_butchery() >= 0);

    if( bones > 0 ) {
        if( corpse->mat == "veggy" ) {
            g->m.spawn_item(p->pos(), "plant_sac", bones, 0, age);
            add_msg(m_good, _("You harvest some fluid bladders!"));
        } else if( corpse->has_flag(MF_BONES) && corpse->has_flag(MF_POISON) ) {
            g->m.spawn_item(p->pos(), "bone_tainted", bones / 2, 0, age);
            add_msg(m_good, _("You harvest some salvageable bones!"));
        } else if( corpse->has_flag(MF_BONES) && corpse->has_flag(MF_HUMAN) ) {
            g->m.spawn_item(p->pos(), "bone_human", bones, 0, age);
            add_msg(m_good, _("You harvest some salvageable bones!"));
        } else if( corpse->has_flag(MF_BONES) ) {
            g->m.spawn_item(p->pos(), "bone", bones, 0, age);
            add_msg(m_good, _("You harvest some usable bones!"));
        }
    }

    if( sinews > 0 ) {
        if( corpse->has_flag(MF_BONES) && !corpse->has_flag(MF_POISON) ) {
            g->m.spawn_item(p->pos(), "sinew", sinews, 0, age);
            add_msg(m_good, _("You harvest some usable sinews!"));
        } else if( corpse->mat == "veggy" ) {
            g->m.spawn_item(p->pos(), "plant_fibre", sinews, 0, age);
            add_msg(m_good, _("You harvest some plant fibers!"));
        }
    }

    if( stomach ) {
        if (corpse->mat != "veggy" && !corpse->has_flag(MF_POISON) && 
            !corpse->has_flag(MF_HUMAN)) {
            const itype_id meat = corpse->get_meat_itype();
            if ((corpse->size == MS_SMALL || corpse->size == MS_MEDIUM) && meat == "meat") {
                g->m.spawn_item(p->pos(), "stomach", 1, 0, age);
                add_msg(m_good, _("You harvest the stomach!"));
            } else if ((corpse->size == MS_LARGE || corpse->size == MS_HUGE) && meat == "meat") {
                g->m.spawn_item(p->pos(), "stomach_large", 1, 0, age);
                add_msg(m_good, _("You harvest the stomach!"));
            }
        }
    }

    if( (corpse->has_flag(MF_FUR) || corpse->has_flag(MF_LEATHER) ||
         corpse->has_flag(MF_CHITIN)) && skins > 0 ) {
        add_msg(m_good, _("You manage to skin the %s!"), corpse->nname().c_str());
        int fur = 0;
        int leather = 0;
        int chitin = 0;

        while (skins > 0 ) {
            if( corpse->has_flag(MF_CHITIN) ) {
                chitin = rng(0, skins);
                skins -= chitin;
                skins = std::max(skins, 0);
            }
            if( corpse->has_flag(MF_FUR) ) {
                fur = rng(0, skins);
                skins -= fur;
                skins = std::max(skins, 0);
            }
            if( corpse->has_flag(MF_LEATHER) ) {
                leather = rng(0, skins);
                skins -= leather;
                skins = std::max(skins, 0);
            }
        }

        if( chitin ) {
            g->m.spawn_item(p->pos(), "chitin_piece", chitin, 0, age);
        }
        if( fur ) {
            g->m.spawn_item(p->pos(), "raw_fur", fur, 0, age);
        }
        if( leather ) {
            g->m.spawn_item(p->pos(), "raw_leather", leather, 0, age);
        }
    }

    if( feathers > 0 ) {
        if( corpse->has_flag(MF_FEATHER) ) {
            g->m.spawn_item(p->pos(), "feather", feathers, 0, age);
            add_msg(m_good, _("You harvest some feathers!"));
        }
    }

    if( fats > 0 ) {
        if( corpse->has_flag(MF_FAT) && corpse->has_flag(MF_POISON) ) {
            g->m.spawn_item(p->pos(), "fat_tainted", fats, 0, age);
            add_msg(m_good, _("You harvest some gooey fat!"));
        } else if( corpse->has_flag(MF_FAT) ) {
            g->m.spawn_item(p->pos(), "fat", fats, 0, age);
            add_msg(m_good, _("You harvest some fat!"));
        }
    }

    //Add a chance of CBM recovery. For shocker and cyborg corpses.
    //As long as the factor is above -4 (the sinew cutoff), you will be able to extract cbms
    bool any_cbm = false;
    bool cbm = false;
    if( corpse->has_flag(MF_CBM_CIV) ) {
        if( roll_butchery() >= 0 ) {
            any_cbm = true;
            cbm = butcher_cbm_item( "bio_power_storage", p->pos3(), age ) || cbm;
        }

        if( roll_butchery() >= 0 ) {
            any_cbm = true;
            cbm = butcher_cbm_group( "bionics_common", p->pos3(), age ) || cbm;
        }
    }

    // Zombie scientist bionics
    if( corpse->has_flag(MF_CBM_SCI) ) {
        if( roll_butchery() >= 0 ) {
            any_cbm = true;
            cbm = butcher_cbm_item( "bio_power_storage", p->pos3(), age ) || cbm;
        }

        if( roll_butchery() >= 0 ) {
            any_cbm = true;
            cbm = butcher_cbm_group( "bionics_sci", p->pos3(), age ) || cbm;
        }
    }

    // Zombie technician bionics
    if( corpse->has_flag(MF_CBM_TECH) ) {
        if( roll_butchery() >= 0 ) {
            any_cbm = true;
            cbm = butcher_cbm_item( "bio_power_storage", p->pos3(), age ) || cbm;
        }

        if( roll_butchery() >= 0 ) {
            any_cbm = true;
            cbm = butcher_cbm_group( "bionics_tech", p->pos3(), age ) || cbm;
        }
    }

    // Substation mini-boss bionics
    if( corpse->has_flag(MF_CBM_SUBS) ) {
        if( roll_butchery() >= 0 ) {
            any_cbm = true;
            cbm = butcher_cbm_item( "bio_power_storage", p->pos3(), age ) || cbm;
        }

        if( roll_butchery() >= 0 ) {
            any_cbm = true;
            cbm = butcher_cbm_group( "bionics_subs", p->pos3(), age ) || cbm;
        }

        if( roll_butchery() >= 0 ) {
            any_cbm = true;
            cbm = butcher_cbm_group( "bionics_subs", p->pos3(), age ) || cbm;
        }
    }

    // Payoff for butchering the zombie bio-op
    if( corpse->has_flag(MF_CBM_OP) ) {
        if( roll_butchery() >= 0 ) {
            any_cbm = true;
            cbm = butcher_cbm_item( "bio_power_storage_mkII", p->pos3(), age ) || cbm;
        }

        if( roll_butchery() >= 0 ) {
            any_cbm = true;
            cbm = butcher_cbm_group( "bionics_op", p->pos3(), age ) || cbm;
        }
    }

    if( cbm ) {
        add_msg( m_good, _("You discover a CBM in the %s!"), corpse->nname().c_str() );
    } else if( any_cbm ) {
        add_msg( m_good, _("You discover a fused lump of bio-circuitry in the %s!"),
                         corpse->nname().c_str() );
    }

    //Add a chance of CBM power storage recovery.
    if( corpse->has_flag(MF_CBM_POWER) ) {
        //As long as the factor is above -4 (the sinew cutoff), you will be able to extract cbms
        if( roll_butchery() >= 0 ) {
            //To see if it spawns a battery
            if( one_in(3) ) { //The battery works 33% of the time.
                add_msg(m_good, _("You discover a power storage in the %s!"), corpse->nname().c_str());
                g->m.spawn_item( p->pos3(), "bio_power_storage", 1, 0, age);
            } else { //There is a burnt out CBM
                add_msg(m_good, _("You discover a fused lump of bio-circuitry in the %s!"),
                        corpse->nname().c_str());
                g->m.spawn_item( p->pos3(), "burnt_out_bionic", 1, 0, age);
            }
        }
    }


    // Recover hidden items
    for( auto &content : contents  ) {
        if( ( roll_butchery() + 10 ) * 5 > rng( 0, 100 ) ) {
            add_msg( m_good, _( "You discover a %s in the %s!" ), content.tname().c_str(),
                     corpse->nname().c_str() );
            g->m.add_item_or_charges( p->pos(), content );
        } else if( content.is_bionic()  ) {
            g->m.spawn_item(p->pos(), "burnt_out_bionic", 1, 0, age);
        }
    }

    if( pieces <= 0 ) {
        add_msg(m_bad, _("Your clumsy butchering destroys the meat!"));
    } else {
        add_msg(m_good, _("You butcher the corpse."));
        const itype_id meat = corpse->get_meat_itype();
        if( meat == "null" ) {
            return;
        }
        item tmpitem(meat, age);
        tmpitem.set_mtype( corpse );
        while ( pieces > 0 ) {
            pieces--;
            g->m.add_item_or_charges(p->pos(), tmpitem);
        }
    }
}

void activity_handlers::fill_liquid_do_turn( player_activity *act, player *p )
{
    //Filling a container takes time, not speed
    act->moves_left -= 100;

    item *container = &p->i_at(act->position);
    item water = item(act->str_values[0], act->values[1]);
    water.poison = act->values[0];
    // Fill up 10 charges per time
    water.charges = 10;

    if( g->handle_liquid(water, true, true, NULL, container) == false ) {
        act->moves_left = 0;
    }

    p->rooted();
    p->pause();
}

// handles equipping an item on ACT_PICKUP, if requested
void activity_handlers::pickup_finish(player_activity *act, player *p)
{
    // loop through all the str_values, and if we find equip, do so.
    // if no str_values present, carry on
    for(auto &elem : act->str_values) {
        if(elem == "equip") {
            item &it = p->i_at(act->position);
            p->wear_item(&it);
        }
    }
}

void activity_handlers::firstaid_finish( player_activity *act, player *p )
{
    item &it = p->i_at(act->position);
    iuse tmp;
    tmp.completefirstaid( p, &it, false, p->pos3() );
    p->reduce_charges(act->position, 1);
    // Erase activity and values.
    act->type = ACT_NULL;
    act->values.clear();
}

// fish-with-rod fish catching function.
static void rod_fish( player *p, int sSkillLevel, int fishChance )
{
    if( sSkillLevel > fishChance ) {
        std::vector<monster *> fishables = g->get_fishable(60); //get the nearby fish list.
        //if the vector is empty (no fish around) the player is still given a small chance to get a (let us say it was hidden) fish
        if( fishables.size() < 1 ) {
            if( one_in(20) ) {
                item fish;
                std::vector<std::string> fish_group = MonsterGroupManager::GetMonstersFromGroup("GROUP_FISH");
                std::string fish_mon = fish_group[rng(1, fish_group.size()) - 1];
                fish.make_corpse( fish_mon, calendar::turn );
                g->m.add_item_or_charges(p->pos(), fish);
                p->add_msg_if_player(m_good, _("You caught a %s."), GetMType(fish_mon)->nname().c_str());
            } else {
                p->add_msg_if_player(_("You didn't catch anything."));
            }
        } else {
            g->catch_a_monster(fishables, p->pos3(), p, 30000);
        }

    } else {
        p->add_msg_if_player(_("You didn't catch anything."));
    }
}

void activity_handlers::fish_finish( player_activity *act, player *p )
{
    item &it = p->i_at(act->position);
    int sSkillLevel = 0;
    int fishChance = 20;
    if( it.has_flag("FISH_POOR") ) {
        sSkillLevel = p->skillLevel("survival") + dice(1, 6);
        fishChance = dice(1, 20);
    } else if( it.has_flag("FISH_GOOD") ) {
        // Much better chances with a good fishing implement.
        sSkillLevel = p->skillLevel("survival") * 1.5 + dice(1, 6) + 3;
        fishChance = dice(1, 20);
    }
    rod_fish( p, sSkillLevel, fishChance );
    p->practice("survival", rng(5, 15));
    act->type = ACT_NULL;
}

void activity_handlers::forage_finish( player_activity *act, player *p )
{
    int veggy_chance = rng(1, 100);
    bool found_something = false;

    if( one_in(12) ) {
        add_msg(m_good, _("You found some trash!"));
        g->m.put_items_from_loc( "trash_forest", p->pos3(), calendar::turn );
        found_something = true;
    }
    items_location loc;
    ter_id next_ter = 0; //Just to have a default for compilers' sake
    switch (calendar::turn.get_season() ) {
    case SPRING:
        loc = "forage_spring";
        next_ter = terfind("t_underbrush_harvested_spring");
        break;
    case SUMMER:
        loc = "forage_summer";
        next_ter = terfind("t_underbrush_harvested_summer");
        break;
    case AUTUMN:
        loc = "forage_autumn";
        next_ter = terfind("t_underbrush_harvested_autumn");
        break;
    case WINTER:
        loc = "forage_winter";
        next_ter = terfind("t_underbrush_harvested_winter");
        break;
    }
    
    // Compromise: Survival gives a bigger boost, and Peception is leveled a bit.
    if( veggy_chance < ((p->skillLevel("survival") * 1.5) + ((p->per_cur / 2 - 4) + 3)) ) {                
        // Returns zero if location has no defined items.
        int cnt = g->m.put_items_from_loc( loc, p->pos3(), calendar::turn );
        if( cnt > 0 ) {
            add_msg(m_good, _("You found something!"));
            g->m.ter_set( act->placement, next_ter );
            found_something = true;
        }
    } else {
        if( one_in(2) ) {
            g->m.ter_set( act->placement, next_ter );
        }
    }
    if( !found_something ) {
        add_msg(_("You didn't find anything."));
    }

    //Determinate maximum level of skill attained by foraging using ones intelligence score
    int max_forage_skill = 0;
    if( p->int_cur < 4 ) {
        max_forage_skill = 1;
    } else if( p->int_cur < 6 ) {
        max_forage_skill = 2;
    } else if( p->int_cur < 8 ) {
        max_forage_skill = 3;
    } else if( p->int_cur < 11 ) {
        max_forage_skill = 4;
    } else if( p->int_cur < 15 ) {
        max_forage_skill = 5;
    } else if( p->int_cur < 20 ) {
        max_forage_skill = 6;
    } else if( p->int_cur < 26 ) {
        max_forage_skill = 7;
    } else if( p->int_cur > 25 ) {
        max_forage_skill = 8;
    }

    const int max_exp {(max_forage_skill * 2) - (p->skillLevel("survival") * 2)};
    //Award experience for foraging attempt regardless of success
    p->practice("survival", rng(1, max_exp), max_forage_skill);
}


void activity_handlers::game_do_turn( player_activity *act, player *p )
{
    //Gaming takes time, not speed
    act->moves_left -= 100;

    item &game_item = p->i_at(act->position);

    //Deduct 1 battery charge for every minute spent playing
    if( int(calendar::turn) % 10 == 0 ) {
        game_item.charges--;
        p->add_morale(MORALE_GAME, 1, 100); //1 points/min, almost 2 hours to fill
    }
    if( game_item.charges == 0 ) {
        act->moves_left = 0;
        add_msg(m_info, _("The %s runs out of batteries."), game_item.tname().c_str());
    }

    p->rooted();
    p->pause();
}


void activity_handlers::hotwire_finish( player_activity *act, player *)
{
    //Grab this now, in case the vehicle gets shifted
    vehicle *veh = g->m.veh_at(act->values[0], act->values[1]);
    if( veh ) {
        int mech_skill = act->values[2];
        if( mech_skill > (int)rng(1, 6) ) {
            //success
            veh->is_locked = false;
            add_msg(_("This wire will start the engine."));
        } else if( mech_skill > (int)rng(0, 4) ) {
            //soft fail
            veh->is_locked = false;
            veh->is_alarm_on = veh->has_security_working();
            add_msg(_("This wire will probably start the engine."));
        } else if( veh->is_alarm_on ) {
            veh->is_locked = false;
            add_msg(_("By process of elimination, this wire will start the engine."));
        } else {
            //hard fail
            veh->is_alarm_on = veh->has_security_working();
            add_msg(_("The red wire always starts the engine, doesn't it?"));
        }
    } else {
        dbg(D_ERROR) << "game:process_activity: ACT_HOTWIRE_CAR: vehicle not found";
        debugmsg("process_activity ACT_HOTWIRE_CAR: vehicle not found");
    }
    act->type = ACT_NULL;
}


void activity_handlers::longsalvage_finish( player_activity *act, player *p )
{
    const static std::string salvage_string = "salvage";
    item &main_tool = p->i_at( act->index );
    auto items = g->m.i_at(p->pos());
    item *salvage_tool = main_tool.get_usable_item( salvage_string );
    if( salvage_tool == nullptr ) {
        debugmsg( "Lost tool used for long salvage" );
        act->type = ACT_NULL;
        return;
    }

    const auto use_fun = salvage_tool->get_use( salvage_string );
    const auto actor = dynamic_cast<const salvage_actor *>( use_fun->get_actor_ptr() );
    if( actor == nullptr ) {
        debugmsg( "iuse_actor type descriptor and actual type mismatch" );
        act->type = ACT_NULL;
        return;
    }

    for( auto it = items.begin(); it != items.end(); ++it ) {
        if( actor->valid_to_cut_up( &*it ) ) {
            actor->cut_up( p, salvage_tool, &*it );
            return;
        }
    }

    add_msg( _("You finish salvaging.") );
    act->type = ACT_NULL;
}


void activity_handlers::make_zlave_finish( player_activity *act, player *p )
{
    static const int full_pulp_threshold = 4;

    auto items = g->m.i_at(p->pos());
    std::string corpse_name = act->str_values[0];
    item *body = NULL;

    for( auto it = items.begin(); it != items.end(); ++it ) {
        if( it->display_name() == corpse_name ) {
            body = &*it;
        }
    }

    if( body == NULL ) {
        add_msg(m_info, _("There's no corpse to make into a zombie slave!"));
        return;
    }

    int success = act->values[0];

    if( success > 0 ) {

        p->practice("firstaid", rng(2, 5));
        p->practice("survival", rng(2, 5));

        p->add_msg_if_player(m_good,
                             _("You slice muscles and tendons, and remove body parts until you're confident the zombie won't be able to attack you when it reainmates."));

        body->set_var( "zlave", "zlave" );
        //take into account the chance that the body yet can regenerate not as we need.
        if( one_in(10) ) {
            body->set_var( "zlave", "mutilated" );
        }

    } else {

        if( success > -20 ) {

            p->practice("firstaid", rng(3, 6));
            p->practice("survival", rng(3, 6));

            p->add_msg_if_player(m_warning,
                                 _("You hack into the corpse and chop off some body parts.  You think the zombie won't be able to attack when it reanimates."));

            success += rng(1, 20);

            if( success > 0 && !one_in(5) ) {
                body->set_var( "zlave", "zlave" );
            } else {
                body->set_var( "zlave", "mutilated" );
            }

        } else {

            p->practice("firstaid", rng(1, 8));
            p->practice("survival", rng(1, 8));

            int pulp = rng(1, full_pulp_threshold);

            body->damage += pulp;

            if( body->damage >= full_pulp_threshold ) {
                body->damage = full_pulp_threshold;
                body->active = false;

                p->add_msg_if_player(m_warning, _("You cut up the corpse too much, it is thoroughly pulped."));
            } else {
                p->add_msg_if_player(m_warning,
                                     _("You cut into the corpse trying to make it unable to attack, but you don't think you have it right."));
            }
        }
    }
}

void activity_handlers::pickaxe_do_turn(player_activity *act, player *p)
{
    const tripoint &pos = act->placement;
    if( calendar::turn % MINUTES(1) == 0 ) { // each turn is too much
        //~ Sound of a Pickaxe at work!
        sounds::sound(pos, 30, _("CHNK! CHNK! CHNK!"));
        if( act->moves_left <= 91000 && act->moves_left > 89000 ) {
            p->add_msg_if_player(m_info,
                                 _("Ugh.  You figure it'll take about an hour and a half at this rate."));
        }
        if( act->moves_left <= 71000 && act->moves_left > 69000 ) {
            p->add_msg_if_player(m_info, _("If it keeps up like this, you might be through in an hour."));
        }
        if( act->moves_left <= 31000 && act->moves_left > 29000 ) {
            p->add_msg_if_player(m_info,
                                 _("Feels like you're making good progress.  Another half an hour, maybe?"));
        }
        if( act->moves_left <= 11000 && act->moves_left > 9000 ) {
            p->add_msg_if_player(m_info, _("That's got it.  Ten more minutes of work and it's open."));
        }
    }
}

void activity_handlers::pickaxe_finish(player_activity *act, player *p)
{
    const tripoint &pos = act->placement;
    item *it = &p->i_at(act->position);
    if( g->m.is_bashable(pos) && g->m.has_flag("SUPPORTS_ROOF", pos) &&
        g->m.ter(pos) != t_tree ) {
        // Tunneling through solid rock is hungry, sweaty, tiring, backbreaking work
        // Betcha wish you'd opted for the J-Hammer ;P
        p->hunger += 15;
        if( p->has_trait("STOCKY_TROGLO") ) {
            p->fatigue += 20; // Yep, dwarves can dig longer before tiring
        } else {
            p->fatigue += 30;
        }
        p->thirst += 15;
        p->mod_pain(2 * rng(1, 3));
        // Mining is construction work!
        p->practice("carpentry", 5);
    } else if( g->m.move_cost(pos) == 2 && g->get_levz() == 0 &&
               g->m.ter(pos) != t_dirt && g->m.ter(pos) != t_grass ) {
        //Breaking up concrete on the surface? not nearly as bad
        p->hunger += 5;
        p->fatigue += 10;
        p->thirst += 5;
    }
    g->m.destroy( pos, true );
    it->charges = std::max(long(0), it->charges - it->type->charges_to_use());
    if( it->charges == 0 && it->destroyed_at_zero_charges() ) {
        p->i_rem(act->position);
    }
}

void activity_handlers::pulp_do_turn( player_activity *act, player *p )
{
    const tripoint &pos = act->placement;
    static const int full_pulp_threshold = 4;
    const int move_cost = int(p->weapon.is_null() ? 80 : p->weapon.attack_time() * 0.8);

    // numbers logic: a str 8 character with a butcher knife (4 bash, 18 cut)
    // should have at least a 50% chance of damaging an intact zombie corpse (75 volume).
    // a str 8 character with a baseball bat (28 bash, 0 cut) should have around a 25% chance.

    int cut_power = p->weapon.type->melee_cut;
    // stabbing weapons are a lot less effective at pulping
    if( p->weapon.has_flag("STAB") || p->weapon.has_flag("SPEAR") ) {
        cut_power /= 2;
    }
    double pulp_power = sqrt((double)(p->str_cur + p->weapon.type->melee_dam)) *
        std::min(1.0, sqrt((double)(cut_power + 1)));
    pulp_power = std::min(pulp_power, (double)p->str_cur);
    pulp_power *= 20; // constant multiplier to get the chance right
    int moves = 0;
    int &num_corpses = act->index; // use this to collect how many corpse are pulped
    auto corpse_pile = g->m.i_at(pos);
    for( auto corpse = corpse_pile.begin(); corpse != corpse_pile.end(); ++corpse ) {
        if( !(corpse->is_corpse() && corpse->damage < full_pulp_threshold) ) {
            continue; // no corpse or already pulped
        }
        int damage = pulp_power / corpse->volume();
        //Determine corpse's blood type.
        field_id type_blood = corpse->get_mtype()->bloodType();
        do {
            moves += move_cost;
            const int mod_sta = ( (p->weapon.weight() / 100 ) + 20) * -1;
            p->mod_stat("stamina", mod_sta);
            // Increase damage as we keep smashing,
            // to insure that we eventually smash the target.
            if( x_in_y(pulp_power, corpse->volume())  ) {
                corpse->damage++;
                p->handle_melee_wear();
            }
            // Splatter some blood around
            tripoint tmp = pos;
            if( type_blood != fd_null ) {
                for( tmp.x = pos.x - 1; tmp.x <= pos.x + 1; tmp.x++ ) {
                    for( tmp.y = pos.y - 1; tmp.y <= pos.y + 1; tmp.y++ ) {
                        if( !one_in(damage + 1) && type_blood != fd_null ) {
                            g->m.add_field( tmp, type_blood, 1, 0 );
                        }
                    }
                }
            }
            if( corpse->damage >= full_pulp_threshold ) {
                corpse->damage = full_pulp_threshold;
                corpse->active = false;
                num_corpses++;
            }
            if( moves >= p->moves ) {
                // enough for this turn;
                p->moves -= moves;
                return;
            }
        } while( corpse->damage < full_pulp_threshold );
    }
    // If we reach this, all corpses have been pulped, finish the activity
    act->moves_left = 0;
    // TODO: Factor in how long it took to do the smashing.
    add_msg(ngettext("The corpse is thoroughly pulped.",
                     "The corpses are thoroughly pulped.", num_corpses));
}

void activity_handlers::refill_vehicle_do_turn( player_activity *act, player *p )
{
    vehicle *veh = g->m.veh_at( act->placement );
    if( veh == nullptr ) {  // Vehicle must've moved or something!
        act->moves_left = 0;
        return;
    }
    bool fuel_pumped = false;
    const auto around = closest_tripoints_first( 1, p->pos() );
    for( const auto &p : around ) {
        if( g->m.ter( p ) == t_gas_pump ||
            g->m.ter_at( p ).id == "t_gas_pump_a" ||
            g->m.ter( p ) == t_diesel_pump ) {
            auto maybe_gas = g->m.i_at( p );
            for( auto gas = maybe_gas.begin(); gas != maybe_gas.end(); ) {
                if( gas->type->id == "gasoline" || gas->type->id == "diesel" ) {
                    fuel_pumped = true;
                    int lack = std::min( veh->fuel_capacity(gas->type->id) -
                                         veh->fuel_left(gas->type->id),  200 );
                    if( gas->charges > lack ) {
                        veh->refill(gas->type->id, lack);
                        gas->charges -= lack;
                        act->moves_left -= 100;
                        gas++;
                    } else {
                        add_msg(m_bad, _("With a clang and a shudder, the pump goes silent."));
                        veh->refill (gas->type->id, gas->charges);
                        gas = maybe_gas.erase( gas );
                        act->moves_left = 0;
                    }

                    break;
                }
            }

            if( fuel_pumped ) {
                break;
            }
        }
    }
    if( !fuel_pumped ) {
        // Can't find any fuel, give up.
        debugmsg("Can't find any fuel, cancelling pumping.");
        p->cancel_activity();
        return;
    }
    p->pause();
}

void activity_handlers::reload_finish( player_activity *act, player *p )
{
    item *reloadable = nullptr;
    {
        int reloadable_pos;
        std::stringstream ss(act->name);
        ss >> reloadable_pos;
        reloadable = &p->i_at(reloadable_pos);
    }
    if( reloadable->reload(*p, act->position) ) {
        if( reloadable->is_gun() && reloadable->has_flag("RELOAD_ONE") ) {
            if( reloadable->ammo_type() == "bolt" ) {
                add_msg(_("You insert a bolt into your %s."),
                        reloadable->tname().c_str());
            } else {
                add_msg(_("You insert a cartridge into your %s."),
                        reloadable->tname().c_str());
            }
            p->recoil = std::max(MIN_RECOIL, (MIN_RECOIL + p->recoil) / 2);
        } else {
            add_msg(_("You reload your %s."), reloadable->tname().c_str());
            p->recoil = MIN_RECOIL;
        }
    } else {
        add_msg(m_info, _("Can't reload your %s."), reloadable->tname().c_str());
    }
    act->type = ACT_NULL;
}

void activity_handlers::start_fire_finish( player_activity *act, player *p )
{
    item &it = p->i_at(act->position);
    firestarter_actor::resolve_firestarter_use( p, &it, act->placement );
    act->type = ACT_NULL;
}

void activity_handlers::start_fire_lens_do_turn( player_activity *act, player *p )
{
    float natural_light_level = g->natural_light_level();
    // if the weather changes, we cannot start a fire with a lens. abort activity
    if( !((g->weather == WEATHER_CLEAR) || (g->weather == WEATHER_SUNNY)) ||
        !( natural_light_level >= 60 ) ) {
        add_msg(m_bad, _("There is not enough sunlight to start a fire now. You stop trying."));
        p->cancel_activity();
    } else if( natural_light_level != act->values.back() ) {
        // when lighting changes we recalculate the time needed
        float previous_natural_light_level = act->values.back();
        act->values.pop_back();
        act->values.push_back(natural_light_level);
        item &lens_item = p->i_at(act->position);
        const auto usef = lens_item.type->get_use( "extended_firestarter" );
        if( usef == nullptr || usef->get_actor_ptr() == nullptr ) {
            add_msg( m_bad, "You have lost the item you were using as a lens." );
            p->cancel_activity();
            return;
        }

        const auto actor = dynamic_cast< const extended_firestarter_actor* >( usef->get_actor_ptr() );
        float progress_left = float(act->moves_left) /
                              float(actor->calculate_time_for_lens_fire(p, previous_natural_light_level));
        act->moves_left = int(progress_left *
                              (actor->calculate_time_for_lens_fire(p, natural_light_level)));
    }
}

void activity_handlers::train_finish( player_activity *act, player *p )
{
    const Skill *skill = Skill::skill(act->name);
    if( skill == NULL ) {
        auto &mastyle = matype_id( act->name ).obj();
        // Trained martial arts,
        add_msg(m_good, _("You learn %s."), mastyle.name.c_str());
        //~ %s is martial art
        p->add_memorial_log(pgettext("memorial_male", "Learned %s."),
                            pgettext("memorial_female", "Learned %s."),
                            mastyle.name.c_str());
        p->add_martialart( mastyle.id );
    } else {
        int new_skill_level = p->skillLevel(skill) + 1;
        p->skillLevel(skill).level(new_skill_level);
        add_msg(m_good, _("You finish training %s to level %d."),
                skill->name().c_str(),
                new_skill_level);
        if( new_skill_level % 4 == 0 ) {
            //~ %d is skill level %s is skill name
            p->add_memorial_log(pgettext("memorial_male", "Reached skill level %1$d in %2$s."),
                                pgettext("memorial_female", "Reached skill level %1$d in %2$s."),
                                new_skill_level, skill->name().c_str());
        }
    }
    act->type = ACT_NULL;
}

void activity_handlers::vehicle_finish( player_activity *act, player *)
{
    //Grab this now, in case the vehicle gets shifted
    vehicle *veh = g->m.veh_at(act->values[0], act->values[1]);
    complete_vehicle();
    // complete_vehicle set activity type to NULL if the vehicle
    // was completely dismantled, otherwise the vehicle still exist and
    // is to be examined again.
    if( act->type == ACT_NULL ) {
        return;
    }
    act->type = ACT_NULL;
    if( act->values.size() < 7 ) {
        dbg(D_ERROR) << "game:process_activity: invalid ACT_VEHICLE values: "
                     << act->values.size();
        debugmsg("process_activity invalid ACT_VEHICLE values:%d",
                 act->values.size());
    } else {
        if( veh ) {
            g->refresh_all();
            // TODO: Z (and also where the activity is queued)
            // Or not, because the vehicle coords are dropped anyway
            g->exam_vehicle(*veh,
                            tripoint( act->values[0], act->values[1], g->get_levz() ),
                            act->values[2], act->values[3]);
            return;
        } else {
            dbg(D_ERROR) << "game:process_activity: ACT_VEHICLE: vehicle not found";
            debugmsg("process_activity ACT_VEHICLE: vehicle not found");
        }
    }
}

void activity_handlers::vibe_do_turn( player_activity *act, player *p )
{
    //Using a vibrator takes time, not speed
    act->moves_left -= 100;

    item &vibrator_item = p->i_at(act->position);

    if( (p->is_wearing("rebreather")) || (p->is_wearing("rebreather_xl")) ||
        (p->is_wearing("mask_h20survivor")) ) {
        act->moves_left = 0;
        add_msg(m_bad, _("You have trouble breathing, and stop."));
    }

    //Deduct 1 battery charge for every minute using the vibrator
    if( int(calendar::turn) % 10 == 0 ) {
        vibrator_item.charges--;
        p->add_morale(MORALE_FEELING_GOOD, 4, 320); //4 points/min, one hour to fill
        // 1:1 fatigue:morale ratio, so maxing the morale is possible but will take
        // you pretty close to Dead Tired from a well-rested state.
        p->fatigue += 4;
    }
    if( vibrator_item.charges == 0 ) {
        act->moves_left = 0;
        add_msg(m_info, _("The %s runs out of batteries."), vibrator_item.tname().c_str());
    }
    if( p->fatigue >= 383 ) { // Dead Tired: different kind of relaxation needed
        act->moves_left = 0;
        add_msg(m_info, _("You're too tired to continue."));
    }

    // Vibrator requires that you be able to move around, stretch, etc, so doesn't play
    // well with roots.  Sorry.  :-(

    p->pause();
}

void activity_handlers::start_engines_finish( player_activity *act, player *p )
{
    // Find the vehicle by looking for a remote vehicle first, then by player relative coords
    vehicle *veh = g->remoteveh();
    if( !veh ) {
        const tripoint pos = act->placement + g->u.pos();
        veh = g->m.veh_at( pos );
        if( !veh ) { return; }
    }

    int attempted = 0;
    int started = 0;
    int not_muscle = 0;
    const bool take_control = act->values[0];

    for( size_t e = 0; e < veh->engines.size(); ++e ) {
        if( veh->is_engine_on( e ) ) {
            attempted++;
            if( veh->start_engine( e ) ) { started++; }
            if( !veh->is_engine_type( e, "muscle" ) ) { not_muscle++; }
        }
    }

    veh->engine_on = attempted > 0 && started == attempted;

    if( attempted == 0 ) {
        add_msg( m_info, _("The %s doesn't have an engine!"), veh->name.c_str() );
    } else if( not_muscle > 0 ) {
        if( started == attempted ) {
            add_msg( ngettext("The %s's engine starts up.",
                "The %s's engines start up.", not_muscle), veh->name.c_str() );
        } else {
            add_msg( m_bad, ngettext("The %s's engine fails to start.",
                "The %s's engines fail to start.", not_muscle), veh->name.c_str() );
        }
    }

    if( take_control && !veh->engine_on && !veh->velocity ) {
        p->controlling_vehicle = false;
        add_msg(_("You let go of the controls."));
    }
}

void activity_handlers::oxytorch_do_turn( player_activity *act, player *p )
{
    item &it = p->i_at( act->position );
    // act->values[0] is the number of charges yet to be consumed
    const int charges_used = std::min( act->values[0], it.type->charges_to_use() );

    it.charges -= charges_used;
    act->values[0] -= charges_used;

    if( calendar::turn % 2 ) {
        sounds::sound( act->placement, 10, _("hissssssssss!") );
    }
}

void activity_handlers::oxytorch_finish( player_activity *act, player *p )
{
    const tripoint &pos = act->placement;
    const ter_id ter = g->m.ter( pos );

    // fast players might still have some charges left to be consumed
    p->i_at( act->position ).charges -= act->values[0];

    if( g->m.furn( pos ) == f_rack ) {
        g->m.furn_set( pos, f_null );
        g->m.spawn_item( p->pos(), "steel_chunk", rng(2, 6) );
    } else if( ter == t_chainfence_v || ter == t_chainfence_h || ter == t_chaingate_c ||
        ter == t_chaingate_l ) {
        g->m.ter_set( pos, t_dirt  );
        g->m.spawn_item( pos, "pipe", rng(1, 4) );
        g->m.spawn_item( pos, "wire", rng(4, 16) );
    } else if( ter == t_chainfence_posts ) {
        g->m.ter_set( pos, t_dirt );
        g->m.spawn_item( pos, "pipe", rng(1, 4) );
    } else if( ter == t_door_metal_locked || ter == t_door_metal_c || ter == t_door_bar_c ||
               ter == t_door_bar_locked || ter == t_door_metal_pickable ) {
        g->m.ter_set( pos, t_mdoor_frame );
        g->m.spawn_item( pos, "steel_plate", rng(0, 1) );
        g->m.spawn_item( pos, "steel_chunk", rng(3, 8) );
    } else if( ter == t_window_enhanced || ter == t_window_enhanced_noglass ) {
        g->m.ter_set( pos, t_window_empty  );
        g->m.spawn_item( pos, "steel_plate", rng(0, 1) );
        g->m.spawn_item( pos, "sheet_metal", rng(1, 3) );
    } else if( ter == t_bars ) {
        if (g->m.ter( {pos.x + 1, pos.y, pos.z} ) == t_sewage || g->m.ter( {pos.x, pos.y + 1, pos.z} ) == t_sewage ||
            g->m.ter( {pos.x - 1, pos.y, pos.z} ) == t_sewage || g->m.ter( {pos.x, pos.y - 1, pos.z} ) == t_sewage) {
            g->m.ter_set( pos, t_sewage );
            g->m.spawn_item( p->pos(), "pipe", rng(1, 2) );
        } else {
            g->m.ter_set( pos, t_floor );
            g->m.spawn_item( p->pos(), "pipe", rng(1, 2) );
        }
    } else if( ter == t_window_bars_alarm ) {
        g->m.ter_set( pos, t_window_empty );
        g->m.spawn_item( p->pos(), "pipe", rng(1, 2) );
    }
}
